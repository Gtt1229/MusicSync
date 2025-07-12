#pragma once
#include "pch.h"

class MusicSync;

struct image {
    std::shared_ptr<ImageWrapper> img;
    Vector2 position;
    float scale;
    LinearColor color;
};

class MusicOverlay {
private:
    std::shared_ptr<GameWrapper> gameWrapper;
    std::shared_ptr<CVarManagerWrapper> cvarManager;
    MusicSync* musicSync;
    inline static std::filesystem::path dataDir;
    inline static std::filesystem::path coverPath;
    inline static auto coverFile = "cover.png";
    // Overlay settings
    std::shared_ptr<bool> enabled;
    std::shared_ptr<float> overlayScale;
    std::shared_ptr<float> overlayX;
    std::shared_ptr<float> overlayY;
    std::shared_ptr<bool> showAlbumCover;

    // Album cover image (using ImageWrapper)
    std::shared_ptr<ImageWrapper> albumCoverImage;
    std::string lastAlbumCoverPath;

    // Cached render data
    std::vector<image> toRender;

public:
    bool needsUpdate = true;

    MusicOverlay(std::shared_ptr<GameWrapper> gw, std::shared_ptr<CVarManagerWrapper> cv, MusicSync* ms);
    ~MusicOverlay();

    void InitializeSettings();
    void UpdateRenderData();
    void RenderOverlay(CanvasWrapper canvas);
    void OnUnload();
    void ClearAlbumCover();

    std::pair<int, int> ParseResolution(const std::string& resolution);
};