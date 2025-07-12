#pragma once

#include "GuiBase.h"
#include "rendering/Overlay.h"
#include "bakkesmod/plugin/bakkesmodplugin.h"
#include "bakkesmod/plugin/pluginwindow.h"
#include "bakkesmod/plugin/PluginSettingsWindow.h"

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Media.Control.h>
#include <winrt/Windows.Storage.Streams.h>
#include <thread>
#include <chrono>
#include <vector>
#include <future>
#include <memory>
#include <mutex>
#include <filesystem>
#include <fstream>

#include "version.h"
#include "pch.h"

constexpr auto plugin_version = stringify(VERSION_MAJOR) "." stringify(VERSION_MINOR) "." stringify(VERSION_PATCH) "." stringify(VERSION_BUILD);

struct MediaInfo {
	std::string title;
	std::string artist;
	std::string album;
	std::string albumCoverPath;
	bool isValid = false;
	bool hasThumbnail = false;

	// Comparison operator for detecting changes
	bool operator==(const MediaInfo& other) const {
		return title == other.title &&
			artist == other.artist &&
			album == other.album &&
			isValid == other.isValid;
	}

	bool operator!=(const MediaInfo& other) const {
		return !(*this == other);
	}
};

class MusicSync : public BakkesMod::Plugin::BakkesModPlugin, public PluginWindowBase, public SettingsWindowBase
{
private:
	std::shared_ptr<bool> enabled;
	MediaInfo currentMediaInfo;
	MediaInfo previousMediaInfo; // Track previous state
	std::mutex mediaInfoMutex;
	std::thread mediaUpdateThread;
	std::atomic<bool> shouldStopThread{ false };
	std::unique_ptr<MusicOverlay> overlay;
	// Simple file paths
	inline static auto coverFile = "cover.png";
	inline static std::filesystem::path dataDir;
	inline static std::filesystem::path coverPath;

	// Album cover file saving
	void SaveAlbumCoverToFile(
		winrt::Windows::Storage::Streams::IRandomAccessStreamReference thumbnail,
		const std::string& fileName);
	void CleanupOldAlbumCovers();
	void InitializePaths();

	// Media control methods
	MediaInfo GetCurrentMediaInfoSync();
	void UpdateMediaInfo();
	void StartMediaUpdateThread();
	void StopMediaUpdateThread();

	// Scoreboard state tracking
	std::atomic<bool> isScoreboardVisible{false};

	// Screen resolution members
	int screenWidth = 1920;
	int screenHeight = 1080;

public:
	void onLoad() override;
	void onUnload() override;
	MediaInfo GetCurrentMedia();
	void RenderCanvas(CanvasWrapper canvas);

	// Scoreboard event handlers
	void openScoreboard(std::string eventName);
	void closeScoreboard(std::string eventName);

	void RenderSettings() override;
	bool isWindowOpen_ = false;
	std::string menuTitle_ = "MusicSync";
	std::string GetPluginName() override;

	std::string GetMenuName() override;
	std::string GetMenuTitle() override;
	void SetImGuiContext(uintptr_t ctx) override;
	bool ShouldBlockInput() override;
	bool IsActiveOverlay() override;
	void OnOpen() override;
	void OnClose() override;
	void Render() override;

	// ParseResolution
	std::pair<int, int> ParseResolution(const std::string& resolution);
};