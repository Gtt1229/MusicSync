#include "pch.h"
#include "Overlay.h"
#include <d3d11.h>
#include <wincodec.h>
#include <filesystem>
#include "../MusicSync.h"
#include <algorithm>

MusicOverlay::MusicOverlay(std::shared_ptr<GameWrapper> gw, std::shared_ptr<CVarManagerWrapper> cv, MusicSync* ms)
    : gameWrapper(gw), cvarManager(cv), musicSync(ms)
{
    InitializeSettings();
}

MusicOverlay::~MusicOverlay()
{
    OnUnload();
}

void MusicOverlay::InitializeSettings()
{
    // Get references to the already-registered CVars
    CVarWrapper enabledCvar = cvarManager->getCvar("music_overlay_enabled");
    CVarWrapper scaleCvar = cvarManager->getCvar("music_overlay_scale");
    CVarWrapper xCvar = cvarManager->getCvar("music_overlay_x");
    CVarWrapper yCvar = cvarManager->getCvar("music_overlay_y");
    CVarWrapper showCoverCvar = cvarManager->getCvar("music_overlay_show_cover");

    // Bind to shared_ptr variables - X/Y are now float for percentages
    enabled = std::make_shared<bool>(enabledCvar ? enabledCvar.getBoolValue() : true);
    overlayScale = std::make_shared<float>(scaleCvar ? scaleCvar.getFloatValue() : 1.0f);
    overlayX = std::make_shared<float>(xCvar ? xCvar.getFloatValue() : 60.0f);
    overlayY = std::make_shared<float>(yCvar ? yCvar.getFloatValue() : 83.0f);
    showAlbumCover = std::make_shared<bool>(showCoverCvar ? showCoverCvar.getBoolValue() : true);

    // Bind them to the CVars for automatic updates
    if (enabledCvar) enabledCvar.bindTo(enabled);
    if (scaleCvar) scaleCvar.bindTo(overlayScale);
    if (xCvar) xCvar.bindTo(overlayX);
    if (yCvar) yCvar.bindTo(overlayY);
    if (showCoverCvar) showCoverCvar.bindTo(showAlbumCover);
}

void MusicOverlay::UpdateRenderData()
{
    toRender.clear();
    
    MediaInfo info = musicSync->GetCurrentMedia();
    if (!info.isValid) {
        needsUpdate = false;
        return;
    }
    
    // Get current screen resolution
    SettingsWrapper settingsWrapper = gameWrapper->GetSettings();
    std::string resolution = settingsWrapper.GetVideoSettings().Resolution;
    auto [screenWidth, screenHeight] = musicSync->ParseResolution(resolution);
    
    // Convert percentages to actual pixel positions
    float scale = *overlayScale;
    int baseX = static_cast<int>(((*overlayX) / 100.0f) * screenWidth);
    int baseY = static_cast<int>(((*overlayY) / 100.0f) * screenHeight);
    
    // Load album cover if needed
    if (*showAlbumCover) {
        dataDir = gameWrapper->GetDataFolder() / "MusicSync";
        coverPath = dataDir / coverFile;
        std::string staticAlbumCoverPath = coverPath.string();
        
        if (lastAlbumCoverPath != staticAlbumCoverPath || !albumCoverImage) {
            try {
                albumCoverImage = std::make_shared<ImageWrapper>(staticAlbumCoverPath, true, false);
                lastAlbumCoverPath = staticAlbumCoverPath;
            }
            catch (const std::exception& e) {
                albumCoverImage.reset();
            }
        }
        
        if (albumCoverImage) {
            float coverScale = scale;
            Vector2 imgSize = albumCoverImage->GetSize();

            image albumImg;
            albumImg.img = albumCoverImage;
            albumImg.position = Vector2{baseX, baseY};
            albumImg.scale = coverScale;
            albumImg.color = LinearColor{1.0f, 1.0f, 1.0f, 1.0f};
            toRender.push_back(albumImg);
        }
    }
    
    needsUpdate = false;
}

void MusicOverlay::RenderOverlay(CanvasWrapper canvas)
{
    CVarWrapper overlayTextColorCvar = cvarManager->getCvar("music_overlay_text_color");
    if (!overlayTextColorCvar) { return; }
    CVarWrapper overlayBkgColorCvar = cvarManager->getCvar("music_overlay_background_color");
    if (!overlayBkgColorCvar) { return; }
    CVarWrapper overlayBkgOpacityCvar = cvarManager->getCvar("music_overlay_background_opacity");
    if (!overlayBkgOpacityCvar) { return; }

    LinearColor textColor = overlayTextColorCvar.getColorValue();
    LinearColor bkgColor = overlayBkgColorCvar.getColorValue();
    int bkgOpacity = overlayBkgOpacityCvar.getIntValue();
    
    if (!*enabled) return;
    
    MediaInfo info = musicSync->GetCurrentMedia();
    if (!info.isValid) return;
    
    // Update render data if needed
    if (needsUpdate) {
        UpdateRenderData();
    }
    
    // Get current screeen resolution for percentage calculation
    SettingsWrapper settingsWrapper = gameWrapper->GetSettings();
    std::string resolution = settingsWrapper.GetVideoSettings().Resolution;
    auto [screenWidth, screenHeight] = musicSync->ParseResolution(resolution);
    
    // Convert percentages to actual pixel positions
    float scale = *overlayScale;
    int baseX = static_cast<int>(((*overlayX) / 100.0f) * screenWidth);
    int baseY = static_cast<int>(((*overlayY) / 100.0f) * screenHeight);
    
    int lineHeight = static_cast<int>(50 * scale);
    int padding = static_cast<int>(20 * scale);
    
    // Calculate background dimensions using the scale
    int albumCoverWidth = 0;
    int albumCoverHeight = 0;
    float actualAlbumCoverScale = 0.2f * scale;
    
    if (!toRender.empty()) {
        auto& img = toRender[0];
        if (img.img && img.img->IsLoadedForCanvas()) {
            Vector2 imgSize = img.img->GetSize();
            float xScale = 1.0;
            float yScale = 1.0;
            if (imgSize.X <= 544.0) {
                xScale = 544.0 / imgSize.X;
            }
            if (imgSize.Y <= 544.0) {
                yScale = 544.0 / imgSize.Y;
            }
            if (yScale < xScale) {
                actualAlbumCoverScale = yScale * actualAlbumCoverScale;
            }
            else {
                actualAlbumCoverScale = xScale * actualAlbumCoverScale;
            }
            albumCoverWidth = static_cast<int>(imgSize.X * actualAlbumCoverScale);
            albumCoverHeight = static_cast<int>(imgSize.Y * actualAlbumCoverScale);
        }
    }
      
    // Calculate total backgground dimensions
    int totalWidth = 650*scale;
    int totalHeight = (108 * scale) + padding;

    // Draw background, album cover, and text
    
    // Draw background rectangle
    canvas.SetColor(bkgColor.R, bkgColor.G, bkgColor.B, bkgOpacity);
    canvas.DrawRect(
        Vector2{baseX - padding, baseY - padding}, 
        Vector2{baseX - padding + totalWidth, baseY - padding + totalHeight}
    );

    // Calculate the content area
    int contentAreaY = baseY;
    int contentAreaHeight = totalHeight - (2 * padding);

    // Calculate album cover vertical centering
    int albumCoverY = contentAreaY + (contentAreaHeight / 2) - (albumCoverHeight / 2);

    // Calculate total text block height
    int textBlockHeight = 0;
    int lineCount = 0;
    if (!info.title.empty()) lineCount++;
    if (!info.artist.empty()) lineCount++;
    if (!info.album.empty()) lineCount++;
    textBlockHeight = lineCount * lineHeight;

    // Calculate text starting position
    int textStartY = contentAreaY + (contentAreaHeight / 2) - (textBlockHeight / 2) + (padding / 2);
    int textX = baseX + albumCoverWidth + padding;

    // Render album cover
    if (*showAlbumCover) {
        if (!toRender.empty()) {
            auto& img = toRender[0];
            if (img.img && img.img->IsLoadedForCanvas()) {
                canvas.SetPosition(Vector2{ baseX, albumCoverY });
                canvas.SetColor(255, 255, 255, 255);
                canvas.DrawTexture(img.img.get(), actualAlbumCoverScale);
            }
        }
    }

    // Render text
    canvas.SetColor(textColor.R, textColor.G, textColor.B, textColor.A);
    int currentY = textStartY;
    float fontSize = 2.0f * scale;

    if (!info.title.empty()) {
        std::string displayTitle = info.title;
        if (displayTitle.length() > 20) {
            displayTitle = displayTitle.substr(0, 20) + "...";
        }
        canvas.SetPosition(Vector2{textX, currentY});
        canvas.DrawString("Title: " + displayTitle, fontSize, fontSize);
        currentY += lineHeight;
    }

    if (!info.artist.empty()) {
        std::string displayArtist = info.artist;
        if (displayArtist.length() > 20) {
            displayArtist = displayArtist.substr(0, 20) + "...";
        }
        canvas.SetPosition(Vector2{textX, currentY});
        canvas.DrawString("By: " + displayArtist, fontSize, fontSize);
        currentY += lineHeight;
    }

    if (!info.album.empty()) {
        canvas.SetPosition(Vector2{textX, currentY});
        canvas.DrawString("From " + info.album, fontSize, fontSize);
    }
}

void MusicOverlay::ClearAlbumCover()
{
    if (albumCoverImage) {
        albumCoverImage.reset();
    }
    toRender.clear();
    lastAlbumCoverPath.clear();
    needsUpdate = true; // Force reload on next render
}

void MusicOverlay::OnUnload()
{
    toRender.clear();
    albumCoverImage.reset();
}