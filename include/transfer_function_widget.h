
#pragma once
#include "ggl.h"

#include "imgui.h"

namespace tfnw {

enum ColorSpace { LINEAR, SRGB };

struct Colormap {
    std::string name;
    // An RGBA8 1D image
    std::vector<uint8_t> colormap;
    ColorSpace color_space;

    Colormap(const std::string &name,
             const std::vector<uint8_t> &img,
             const ColorSpace color_space);
};

class WebGPUTransferFunctionWidget {
    struct vec2f {
        float x, y;

        vec2f(float c = 0.f);
        vec2f(float x, float y);
        vec2f(const ImVec2 &v);

        float length() const;

        vec2f operator+(const vec2f &b) const;
        vec2f operator-(const vec2f &b) const;
        vec2f operator/(const vec2f &b) const;
        vec2f operator*(const vec2f &b) const;
        operator ImVec2() const;
    };

    WGPUDevice device;
    WGPUQueue queue;
    WGPUTexture colormap_texture;
    WGPUTextureView colormap_view;
    WGPUSampler colormap_sampler;
    
    std::vector<Colormap> colormaps;
    size_t selected_colormap = 0;
    std::vector<uint8_t> current_colormap;

    std::vector<vec2f> alpha_control_pts = {vec2f(0.f), vec2f(1.f)};
    size_t selected_point = SIZE_MAX;  // 使用 SIZE_MAX 而不是 -1

    bool clicked_on_item = false;
    bool texture_needs_update = true;
    bool colormap_changed = true;
    
    static constexpr size_t COLORMAP_WIDTH = 256;

public:
    WebGPUTransferFunctionWidget(WGPUDevice device, WGPUQueue queue);
    ~WebGPUTransferFunctionWidget();

    // Add a colormap preset. The image should be a 1D RGBA8 image, if the image
    // is provided in sRGBA colorspace it will be linearized
    void add_colormap(const Colormap &map);

    // Add the transfer function UI into the currently active window
    void draw_ui();

    // Returns true if the colormap was updated since the last
    // call to draw_ui
    bool changed() const;

    // Get back the RGBA8 color data for the transfer function
    std::vector<uint8_t> get_colormap();

    // Get back the RGBA32F color data for the transfer function
    std::vector<float> get_colormapf();

    // Get back the RGBA32F color data for the transfer function
    // as separate color and opacity vectors
    void get_colormapf(std::vector<float> &color, std::vector<float> &opacity);

    // Get the WebGPU texture containing the transfer function
    WGPUTexture get_webgpu_texture() const;
    WGPUTextureView get_webgpu_texture_view() const;
    WGPUSampler get_webgpu_sampler() const;

private:
    void init_webgpu_resources();
    void cleanup_webgpu_resources();
    void update_webgpu_texture();
    void update_colormap();
    void load_embedded_preset(const uint8_t *buf, size_t size, const std::string &name);
    void create_default_colormaps();
};

}