#include "pch.h"
#include "MusicSync.h"

namespace winrt_foundation = winrt::Windows::Foundation;
namespace winrt_media = winrt::Windows::Media::Control;
namespace winrt_streams = winrt::Windows::Storage::Streams;
namespace winrt_storage = winrt::Windows::Storage;

BAKKESMOD_PLUGIN(MusicSync, "Music synchronization plugin that displays current playing song info", plugin_version, PLUGINTYPE_FREEPLAY)

std::shared_ptr<CVarManagerWrapper> _globalCvarManager;

void MusicSync::InitializePaths()
{
	// Get the plugin data directory
	dataDir = gameWrapper->GetDataFolder() / "MusicSync";
	coverPath = dataDir / coverFile;

	// Create directory if it doesn't exist
	std::filesystem::create_directories(dataDir);

	LOG("Data directory: {}", dataDir.string());
	LOG("Cover path: {}", coverPath.string());
}

void MusicSync::onLoad()
{
    _globalCvarManager = cvarManager;
    LOG("MusicSync plugin loaded!");

    // Initialize WinRT
    winrt::init_apartment();

    // Initialize file paths
    InitializePaths();

    // Get screen resolution
    SettingsWrapper settingsWrapper = gameWrapper->GetSettings();
    std::string resolution = settingsWrapper.GetVideoSettings().Resolution;
    LOG("Current resolution: {}", resolution);

    // Parse the resolution and store for later
    auto [width, height] = ParseResolution(resolution);
    screenWidth = width;
    screenHeight = height;
    LOG("Parsed screen dimensions: {}x{}", screenWidth, screenHeight);

    // Calculate initial overlay position as percentages
    float overlayPercentX = 60.0f; // 60% from left edge
    float overlayPercentY = 83.0f; // 83% from top edge

    // Create enabled cvar
    enabled = std::make_shared<bool>(true);
    cvarManager->registerCvar("musicsync_enabled", "1", "Enable the MusicSync plugin", true, true, 0, true, 1).bindTo(enabled);

    // Register overlay CVars with percentage-based positions (0-100)
    cvarManager->registerCvar("music_overlay_enabled", "1", "Enable music overlay", true, true, 0, true, 1);
    cvarManager->registerCvar("music_overlay_scale", "1.0", "Music overlay scale", true, true, 0.5f, true, 2.5f);
    cvarManager->registerCvar("music_overlay_x", std::to_string(overlayPercentX), "Music overlay X position (percentage)", true, true, 0.0f, true, 100.0f);
    cvarManager->registerCvar("music_overlay_y", std::to_string(overlayPercentY), "Music overlay Y position (percentage)", true, true, 0.0f, true, 100.0f);
    cvarManager->registerCvar("music_overlay_show_cover", "1", "Show album cover", true, true, 0, true, 1);
	cvarManager->registerCvar("music_overlay_always_enabled", "0", "Always show overlay", true, true, 0, true, 1);

    // Register color CVars
    cvarManager->registerCvar("music_overlay_text_color", "(255,255,255,255)", "Text color");
    cvarManager->registerCvar("music_overlay_background_color", "(0,0,0,255)", "Background color");
    cvarManager->registerCvar("music_overlay_background_opacity", "100", "Background opacity");

    // Register notifier to get current media info
    cvarManager->registerNotifier("musicsync_get_info", [this](std::vector<std::string> args) {
        MediaInfo info = GetCurrentMedia();
        if (info.isValid) {
            LOG("Current Song: {} - {}", info.artist, info.title);
            if (!info.album.empty()) {
                LOG("Album: {}", info.album);
                if (!info.albumCoverPath.empty()) {
                    LOG("Album cover saved to: {}", info.albumCoverPath);
                }
            }
        }
        else {
            LOG("No media currently playing or accessible");
        }
    }, "Get current media info", PERMISSION_ALL);

    // Scoreboard hook
    gameWrapper->HookEvent("Function TAGame.GFxData_GameEvent_TA.OnOpenScoreboard", 
        std::bind(&MusicSync::openScoreboard, this, std::placeholders::_1));
    
    gameWrapper->HookEvent("Function TAGame.GFxData_GameEvent_TA.OnCloseScoreboard", 
        std::bind(&MusicSync::closeScoreboard, this, std::placeholders::_1));

    // Initialize overlay
    overlay = std::make_unique<MusicOverlay>(gameWrapper, cvarManager, this);
    LOG("MusicSync overlay initialized!");

    // Register canvas drawable
    gameWrapper->RegisterDrawable(std::bind(&MusicSync::RenderCanvas, this, std::placeholders::_1));
    LOG("Canvas rendering drawable registered!");

    // Start the media update thread
    StartMediaUpdateThread();
}

void MusicSync::onUnload()
{
	LOG("MusicSync plugin unloading...");
	
	// Unregister drawable
	gameWrapper->UnregisterDrawables();
	
	// Clean up overlay
	if (overlay) {
		overlay->OnUnload();
		overlay.reset();
		LOG("Overlay cleaned up");
	}
	
	StopMediaUpdateThread();
	CleanupOldAlbumCovers();
	winrt::uninit_apartment();
}

void MusicSync::SaveAlbumCoverToFile(
	winrt::Windows::Storage::Streams::IRandomAccessStreamReference thumbnail,
	const std::string& fileName)
{
	try {
		// Open the thumbnail stream synchronously 
		auto streamOp = thumbnail.OpenReadAsync();
		auto stream = streamOp.get();

		if (stream != nullptr) {
			auto size = stream.Size();
			if (size > 0 && size < 10 * 1024 * 1024) { // Limit to 10MB
				// Read the stream data
				std::vector<uint8_t> buffer(static_cast<size_t>(size));
				auto winrtBuffer = winrt_streams::Buffer(static_cast<uint32_t>(size));
				auto readOp = stream.ReadAsync(winrtBuffer, static_cast<uint32_t>(size), winrt_streams::InputStreamOptions::None);
				auto bytesRead = readOp.get();

				if (bytesRead.Length() > 0) {
					auto dataReader = winrt_streams::DataReader::FromBuffer(bytesRead);
					dataReader.ReadBytes(buffer);

					// Write to file using standard filesystem
					std::ofstream file(coverPath, std::ios::binary);
					if (file.is_open()) {
						file.write(reinterpret_cast<const char*>(buffer.data()), buffer.size());
						file.close();
						//LOG("Album cover saved successfully to: {}", coverPath.string());
					}
					else {
						LOG("Failed to open file for writing: {}", coverPath.string());
					}
				}
			}
		}
		stream.Close();
	}
	catch (const std::exception& e) {
		LOG("Failed to save album cover: {} - Error: {}", fileName, e.what());
	}
	catch (...) {
		LOG("Failed to save album cover: {} - Unknown error", fileName);
	}
}

void MusicSync::CleanupOldAlbumCovers()
{
	try {
		if (std::filesystem::exists(coverPath)) {
			std::filesystem::remove(coverPath);
		}
	}
	catch (...) {
		LOG("Error during album cover cleanup");
	}
}

MediaInfo MusicSync::GetCurrentMediaInfoSync()
{
    MediaInfo info;

    try {
        // Get the session manager
        auto sessionManagerOp = winrt_media::GlobalSystemMediaTransportControlsSessionManager::RequestAsync();
        auto sessionManager = sessionManagerOp.get();
        auto currentSession = sessionManager.GetCurrentSession();

        if (currentSession != nullptr) {
            // Get media properties
            auto mediaPropertiesOp = currentSession.TryGetMediaPropertiesAsync();
            auto mediaProperties = mediaPropertiesOp.get();

            if (mediaProperties != nullptr) {
                info.title = winrt::to_string(mediaProperties.Title());
                info.artist = winrt::to_string(mediaProperties.Artist());
                info.album = winrt::to_string(mediaProperties.AlbumTitle());
                info.isValid = true;

                // Try to get and save album artwork if media has changed
                auto thumbnail = mediaProperties.Thumbnail();
                if (thumbnail != nullptr) {
                    try {
                        info.albumCoverPath = coverPath.string();
                        info.hasThumbnail = true;

                        // Only save if this is different media than before
                        if (info != previousMediaInfo) {
                            LOG("Media changed - saving new album cover");
                            LOG("Current Song: {} - {}", info.artist, info.title);
                            
                            // CLEAR THE OVERLAY IMAGE FIRST to release file lock
                            if (overlay) {
                                overlay->ClearAlbumCover();
                            }
                            
                            std::string fileName = coverFile;

                            // Save the album cover synchronously
                            SaveAlbumCoverToFile(thumbnail, fileName);

                            previousMediaInfo = info;
                            
                        } else {
                            //LOG("Same media playing - skipping album cover save");
                        }
                    }
                    catch (...) {
                        // Failed to save thumbnail
                        info.hasThumbnail = false;
                    }
                }
            }
        }
    }
    catch (const std::exception& e) {
        LOG("Error getting media info: {}", e.what());
    }
    catch (...) {
        LOG("Unknown error getting media info");
    }

    return info;
}

void MusicSync::UpdateMediaInfo()
{
	try {
		auto info = GetCurrentMediaInfoSync();

		std::lock_guard<std::mutex> lock(mediaInfoMutex);

		// Check if media has changed
		bool mediaChanged = (info != currentMediaInfo);
		currentMediaInfo = std::move(info);

		// Trigger overlay update if media changed
		if (overlay && mediaChanged) {
			// Set flag to update render data on next frame
			overlay->needsUpdate = true;
		}
	}
	catch (...) {
		std::lock_guard<std::mutex> lock(mediaInfoMutex);
		currentMediaInfo = MediaInfo{};
	}
}

void MusicSync::StartMediaUpdateThread()
{
	shouldStopThread = false;
	mediaUpdateThread = std::thread([this]() {
		while (!shouldStopThread) {
			if (*enabled) {
				UpdateMediaInfo();
			}

			// Update every 2 seconds
			for (int i = 0; i < 20 && !shouldStopThread; ++i) {
				std::this_thread::sleep_for(std::chrono::milliseconds(100));
			}
		}
		});
}

void MusicSync::StopMediaUpdateThread()
{
	shouldStopThread = true;
	if (mediaUpdateThread.joinable()) {
		mediaUpdateThread.join();
	}
}

MediaInfo MusicSync::GetCurrentMedia()
{
	std::lock_guard<std::mutex> lock(mediaInfoMutex);
	return currentMediaInfo;
}

void MusicSync::RenderCanvas(CanvasWrapper canvas)
{
	// Only render when scoreboard is visible, this actually works in freeplay, may change this to include after match and on main menu
	if (overlay && isScoreboardVisible) {
		overlay->RenderOverlay(canvas);
	}
}

void MusicSync::openScoreboard(std::string eventName)
{
	isScoreboardVisible = true;
	//LOG("Scoreboard opened");
}

void MusicSync::closeScoreboard(std::string eventName)
{
    CVarWrapper alwaysEnabledCvar = cvarManager->getCvar("music_overlay_always_enabled");
    bool alwaysEnabled = alwaysEnabledCvar.getBoolValue();
    if (!alwaysEnabled) {
        isScoreboardVisible = false;
    }
}

std::pair<int, int> MusicSync::ParseResolution(const std::string& resolution)
{
    // Default fallback values
    int width = 1920;
    int height = 1080;

    try {
        // Find the 'x' character that separates width and height
        size_t xPos = resolution.find('x');

        if (xPos != std::string::npos) {
            std::string widthStr = resolution.substr(0, xPos);
            std::string heightStr = resolution.substr(xPos + 1);

            // Convert strings to integers
            width = std::stoi(widthStr);
            height = std::stoi(heightStr);
        }
        else {
            LOG("Invalid resolution format: '{}', using default 1920x1080", resolution);
        }
    }
    catch (const std::exception& e) {
        LOG("Error parsing resolution '{}': {}, using default 1920x1080", resolution, e.what());
    }

    return std::make_pair(width, height);
}