#include "transfer_function_widget.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <cstring>

#include "embedded_colormaps.h"
#include "stb_image.h"

namespace tfnw {

template <typename T>
inline T clamp(T x, T min, T max)
{
    if (x < min) {
        return min;
    }
    if (x > max) {
        return max;
    }
    return x;
}

inline float srgb_to_linear(const float x)
{
    if (x <= 0.04045f) {
        return x / 12.92f;
    } else {
        return std::pow((x + 0.055f) / 1.055f, 2.4f);
    }
}

Colormap::Colormap(const std::string &name,
                   const std::vector<uint8_t> &img,
                   const ColorSpace color_space)
    : name(name), colormap(img), color_space(color_space)
{
}

WebGPUTransferFunctionWidget::vec2f::vec2f(float c) : x(c), y(c) {}

WebGPUTransferFunctionWidget::vec2f::vec2f(float x, float y) : x(x), y(y) {}

WebGPUTransferFunctionWidget::vec2f::vec2f(const ImVec2 &v) : x(v.x), y(v.y) {}

float WebGPUTransferFunctionWidget::vec2f::length() const
{
    return std::sqrt(x * x + y * y);
}

WebGPUTransferFunctionWidget::vec2f WebGPUTransferFunctionWidget::vec2f::operator+(
    const WebGPUTransferFunctionWidget::vec2f &b) const
{
    return vec2f(x + b.x, y + b.y);
}

WebGPUTransferFunctionWidget::vec2f WebGPUTransferFunctionWidget::vec2f::operator-(
    const WebGPUTransferFunctionWidget::vec2f &b) const
{
    return vec2f(x - b.x, y - b.y);
}

WebGPUTransferFunctionWidget::vec2f WebGPUTransferFunctionWidget::vec2f::operator/(
    const WebGPUTransferFunctionWidget::vec2f &b) const
{
    return vec2f(x / b.x, y / b.y);
}

WebGPUTransferFunctionWidget::vec2f WebGPUTransferFunctionWidget::vec2f::operator*(
    const WebGPUTransferFunctionWidget::vec2f &b) const
{
    return vec2f(x * b.x, y * b.y);
}

WebGPUTransferFunctionWidget::vec2f::operator ImVec2() const
{
    return ImVec2(x, y);
}

WebGPUTransferFunctionWidget::WebGPUTransferFunctionWidget(WGPUDevice device, WGPUQueue queue)
    : device(device), queue(queue), colormap_texture(nullptr), colormap_view(nullptr), colormap_sampler(nullptr)
{
    // 首先确保 current_colormap 有正确的大小
    current_colormap.resize(COLORMAP_WIDTH * 4);
    
    init_webgpu_resources();
    
    // 先创建默认颜色映射
    create_default_colormaps();
    
    // 然后尝试加载嵌入的预设（如果有的话）
    try {
        load_embedded_preset(paraview_cool_warm, sizeof(paraview_cool_warm), "ParaView Cool Warm");
        load_embedded_preset(rainbow, sizeof(rainbow), "Rainbow");
        load_embedded_preset(matplotlib_plasma, sizeof(matplotlib_plasma), "Matplotlib Plasma");
        load_embedded_preset(matplotlib_virdis, sizeof(matplotlib_virdis), "Matplotlib Virdis");
        load_embedded_preset(samsel_linear_green, sizeof(samsel_linear_green), "Samsel Linear Green");
        load_embedded_preset(samsel_linear_ygb_1211g, sizeof(samsel_linear_ygb_1211g), "Samsel Linear YGB 1211G");
        load_embedded_preset(cool_warm_extended, sizeof(cool_warm_extended), "Cool Warm Extended");
        load_embedded_preset(blackbody, sizeof(blackbody), "Black Body");
        load_embedded_preset(jet, sizeof(jet), "Jet");
        load_embedded_preset(blue_gold, sizeof(blue_gold), "Blue Gold");
        load_embedded_preset(ice_fire, sizeof(ice_fire), "Ice Fire");
        load_embedded_preset(nic_edge, sizeof(nic_edge), "nic Edge");
    } catch (...) {
        std::cout << "Warning: Could not load embedded presets, using default colormaps only" << std::endl;
    }

    // Initialize the colormap alpha channel w/ a linear ramp
    update_colormap();
}

WebGPUTransferFunctionWidget::~WebGPUTransferFunctionWidget()
{
    cleanup_webgpu_resources();
}

void WebGPUTransferFunctionWidget::init_webgpu_resources()
{
    // Create texture for the colormap (1D texture, 256 pixels wide)
    WGPUTextureDescriptor texture_desc = {};
    texture_desc.label = "Transfer Function Colormap";
    texture_desc.usage = WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopyDst;
    texture_desc.dimension = WGPUTextureDimension_2D;
    texture_desc.size.width = COLORMAP_WIDTH;
    texture_desc.size.height = 1;
    texture_desc.size.depthOrArrayLayers = 1;
    texture_desc.format = WGPUTextureFormat_RGBA8Unorm;
    texture_desc.mipLevelCount = 1;
    texture_desc.sampleCount = 1;
    
    colormap_texture = wgpuDeviceCreateTexture(device, &texture_desc);
    
    // Create texture view
    WGPUTextureViewDescriptor view_desc = {};
    view_desc.label = "Transfer Function Colormap View";
    view_desc.format = WGPUTextureFormat_RGBA8Unorm;
    view_desc.dimension = WGPUTextureViewDimension_2D;
    view_desc.baseMipLevel = 0;
    view_desc.mipLevelCount = 1;
    view_desc.baseArrayLayer = 0;
    view_desc.arrayLayerCount = 1;
    view_desc.aspect = WGPUTextureAspect_All;
    
    colormap_view = wgpuTextureCreateView(colormap_texture, &view_desc);
    
    // Create sampler
    WGPUSamplerDescriptor sampler_desc = {};
    sampler_desc.label = "Transfer Function Sampler";
    sampler_desc.addressModeU = WGPUAddressMode_ClampToEdge;
    sampler_desc.addressModeV = WGPUAddressMode_ClampToEdge;
    sampler_desc.addressModeW = WGPUAddressMode_ClampToEdge;
    sampler_desc.magFilter = WGPUFilterMode_Linear;
    sampler_desc.minFilter = WGPUFilterMode_Linear;
    sampler_desc.mipmapFilter = WGPUMipmapFilterMode_Linear;
    sampler_desc.lodMinClamp = 0.0f;
    sampler_desc.lodMaxClamp = 32.0f;
    sampler_desc.compare = WGPUCompareFunction_Undefined;
    sampler_desc.maxAnisotropy = 1;
    
    colormap_sampler = wgpuDeviceCreateSampler(device, &sampler_desc);
}

void WebGPUTransferFunctionWidget::cleanup_webgpu_resources()
{
    if (colormap_sampler) {
        wgpuSamplerRelease(colormap_sampler);
        colormap_sampler = nullptr;
    }
    if (colormap_view) {
        wgpuTextureViewRelease(colormap_view);
        colormap_view = nullptr;
    }
    if (colormap_texture) {
        wgpuTextureRelease(colormap_texture);
        colormap_texture = nullptr;
    }
}

void WebGPUTransferFunctionWidget::create_default_colormaps()
{
    // Cool-Warm colormap
    {
        std::vector<uint8_t> coolwarm(COLORMAP_WIDTH * 4);
        for (size_t i = 0; i < COLORMAP_WIDTH; ++i) {
            float t = static_cast<float>(i) / (COLORMAP_WIDTH - 1);
            coolwarm[i * 4 + 0] = static_cast<uint8_t>(255 * (0.230f + 0.299f * t + 0.754f * t * t)); // R
            coolwarm[i * 4 + 1] = static_cast<uint8_t>(255 * (0.299f + 0.718f * t - 0.395f * t * t)); // G
            coolwarm[i * 4 + 2] = static_cast<uint8_t>(255 * (0.754f + 0.395f * t - 0.299f * t * t)); // B
            coolwarm[i * 4 + 3] = 255; // A
        }
        add_colormap(Colormap("Cool Warm", coolwarm, LINEAR));
    }
    
    // Rainbow colormap
    {
        std::vector<uint8_t> rainbow(COLORMAP_WIDTH * 4);
        for (size_t i = 0; i < COLORMAP_WIDTH; ++i) {
            float t = static_cast<float>(i) / (COLORMAP_WIDTH - 1);
            float h = t * 360.0f;
            float s = 1.0f;
            float v = 1.0f;
            
            // HSV to RGB conversion
            float c = v * s;
            float x = c * (1 - std::abs(std::fmod(h / 60.0f, 2) - 1));
            float m = v - c;
            
            float r, g, b;
            if (h < 60) { r = c; g = x; b = 0; }
            else if (h < 120) { r = x; g = c; b = 0; }
            else if (h < 180) { r = 0; g = c; b = x; }
            else if (h < 240) { r = 0; g = x; b = c; }
            else if (h < 300) { r = x; g = 0; b = c; }
            else { r = c; g = 0; b = x; }
            
            rainbow[i * 4 + 0] = static_cast<uint8_t>(255 * (r + m)); // R
            rainbow[i * 4 + 1] = static_cast<uint8_t>(255 * (g + m)); // G
            rainbow[i * 4 + 2] = static_cast<uint8_t>(255 * (b + m)); // B
            rainbow[i * 4 + 3] = 255; // A
        }
        add_colormap(Colormap("Rainbow", rainbow, LINEAR));
    }
    
    // Grayscale colormap
    {
        std::vector<uint8_t> grayscale(COLORMAP_WIDTH * 4);
        for (size_t i = 0; i < COLORMAP_WIDTH; ++i) {
            uint8_t gray = static_cast<uint8_t>(255 * i / (COLORMAP_WIDTH - 1));
            grayscale[i * 4 + 0] = gray; // R
            grayscale[i * 4 + 1] = gray; // G
            grayscale[i * 4 + 2] = gray; // B
            grayscale[i * 4 + 3] = 255;  // A
        }
        add_colormap(Colormap("Grayscale", grayscale, LINEAR));
    }
}

void WebGPUTransferFunctionWidget::add_colormap(const Colormap &map)
{
    colormaps.push_back(map);

    if (colormaps.back().color_space == SRGB) {
        Colormap &cmap = colormaps.back();
        cmap.color_space = LINEAR;
        for (size_t i = 0; i < cmap.colormap.size() / 4; ++i) {
            for (size_t j = 0; j < 3; ++j) {
                const float x = srgb_to_linear(cmap.colormap[i * 4 + j] / 255.f);
                cmap.colormap[i * 4 + j] = static_cast<uint8_t>(clamp(x * 255.f, 0.f, 255.f));
            }
        }
    }
}

void WebGPUTransferFunctionWidget::draw_ui()
{
    update_webgpu_texture();

    const ImGuiIO &io = ImGui::GetIO();

    ImGui::Text("Transfer Function");
    ImGui::TextWrapped(
        "Left click to add a point, right click remove. "
        "Left click + drag to move points.");

    if (ImGui::BeginCombo("Colormap", colormaps[selected_colormap].name.c_str())) {
        for (size_t i = 0; i < colormaps.size(); ++i) {
            if (ImGui::Selectable(colormaps[i].name.c_str(), selected_colormap == i)) {
                selected_colormap = i;
                update_colormap();
            }
        }
        ImGui::EndCombo();
    }

    vec2f canvas_size = ImGui::GetContentRegionAvail();
    
    // 确保画布大小合理
    if (canvas_size.x < 50.0f) canvas_size.x = 200.0f;
    if (canvas_size.y < 50.0f) canvas_size.y = 100.0f;
    
    // 绘制颜色条 - 使用 ImGui 绘制命令而不是纹理
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    ImVec2 canvas_pos = ImGui::GetCursorScreenPos();
    const float colorbar_height = 20.0f;
    
    // 获取当前窗口的剪裁矩形
    ImVec2 window_min = ImGui::GetWindowPos();
    ImVec2 window_max = ImVec2(window_min.x + ImGui::GetWindowSize().x, 
                               window_min.y + ImGui::GetWindowSize().y);
    
    // 确保绘制区域在窗口范围内
    ImVec2 safe_canvas_pos = ImVec2(
        std::max(canvas_pos.x, window_min.x),
        std::max(canvas_pos.y, window_min.y)
    );
    ImVec2 safe_canvas_end = ImVec2(
        std::min(canvas_pos.x + canvas_size.x, window_max.x - 10),
        std::min(canvas_pos.y + colorbar_height, window_max.y - 10)
    );
    
    // 重新计算安全的画布大小
    vec2f safe_canvas_size = vec2f(
        std::max(safe_canvas_end.x - safe_canvas_pos.x, 50.0f),
        std::max(safe_canvas_end.y - safe_canvas_pos.y, colorbar_height)
    );
    
    // 绘制颜色条背景
    draw_list->AddRectFilled(safe_canvas_pos, 
                            ImVec2(safe_canvas_pos.x + safe_canvas_size.x, safe_canvas_pos.y + colorbar_height),
                            IM_COL32(50, 50, 50, 255));
    
    // 绘制颜色条 - 使用更密集的采样
    const int num_segments = std::min(256, (int)safe_canvas_size.x);
    if (num_segments > 0) {
        for (int x = 0; x < num_segments; ++x) {
            float t = static_cast<float>(x) / (num_segments - 1);
            int idx = static_cast<int>(t * (COLORMAP_WIDTH - 1));
            idx = clamp(idx, 0, (int)COLORMAP_WIDTH - 1);
            
            uint8_t r = current_colormap[idx * 4 + 0];
            uint8_t g = current_colormap[idx * 4 + 1];
            uint8_t b = current_colormap[idx * 4 + 2];
            uint8_t a = current_colormap[idx * 4 + 3];
            
            ImU32 color = IM_COL32(r, g, b, a);
            
            float x_pos = safe_canvas_pos.x + (t * safe_canvas_size.x);
            float width = safe_canvas_size.x / num_segments + 1.0f; // 稍微重叠避免间隙
            
            // 确保矩形不会超出安全区域
            float rect_end = std::min(x_pos + width, safe_canvas_pos.x + safe_canvas_size.x);
            
            if (rect_end > x_pos) {
                draw_list->AddRectFilled(ImVec2(x_pos, safe_canvas_pos.y),
                                        ImVec2(rect_end, safe_canvas_pos.y + colorbar_height),
                                        color);
            }
        }
    }
    
    // 添加边框
    draw_list->AddRect(safe_canvas_pos, 
                      ImVec2(safe_canvas_pos.x + safe_canvas_size.x, safe_canvas_pos.y + colorbar_height),
                      IM_COL32(180, 180, 180, 255));
    
    ImGui::Dummy(ImVec2(safe_canvas_size.x, colorbar_height + 5));
    
    // Alpha control area
    canvas_pos = ImGui::GetCursorScreenPos();
    canvas_size = vec2f(safe_canvas_size.x, std::max(safe_canvas_size.y - 25, 50.0f));

    const float point_radius = 10.f;

    // 使用安全的剪裁区域
    ImVec2 clip_min = ImVec2(
        std::max(canvas_pos.x, window_min.x),
        std::max(canvas_pos.y, window_min.y)
    );
    ImVec2 clip_max = ImVec2(
        std::min(canvas_pos.x + canvas_size.x, window_max.x - 10),
        std::min(canvas_pos.y + canvas_size.y, window_max.y - 10)
    );

    // 只有在剪裁区域有效时才设置剪裁
    if (clip_max.x > clip_min.x && clip_max.y > clip_min.y) {
        draw_list->PushClipRect(clip_min, clip_max, true);

        const vec2f view_scale(canvas_size.x, -canvas_size.y);
        const vec2f view_offset(canvas_pos.x, canvas_pos.y + canvas_size.y);

        draw_list->AddRect(canvas_pos, ImVec2(canvas_pos.x + canvas_size.x, canvas_pos.y + canvas_size.y), 
                          ImColor(180, 180, 180, 255));

        ImGui::InvisibleButton("tfn_canvas", canvas_size);

        if (!io.MouseDown[0] && !io.MouseDown[1]) {
            clicked_on_item = false;
        }
        if (ImGui::IsItemHovered() && (io.MouseDown[0] || io.MouseDown[1])) {
            clicked_on_item = true;
        }

        ImVec2 bbmin = ImGui::GetItemRectMin();
        ImVec2 bbmax = ImGui::GetItemRectMax();
        ImVec2 clipped_mouse_pos = ImVec2(std::min(std::max(io.MousePos.x, bbmin.x), bbmax.x),
                                          std::min(std::max(io.MousePos.y, bbmin.y), bbmax.y));

        if (clicked_on_item) {
            vec2f mouse_pos = (vec2f(clipped_mouse_pos) - view_offset) / view_scale;
            mouse_pos.x = clamp(mouse_pos.x, 0.f, 1.f);
            mouse_pos.y = clamp(mouse_pos.y, 0.f, 1.f);

            if (io.MouseDown[0]) {
                if (selected_point != SIZE_MAX) {
                    alpha_control_pts[selected_point] = mouse_pos;

                    // Keep the first and last control points at the edges
                    if (selected_point == 0) {
                        alpha_control_pts[selected_point].x = 0.f;
                    } else if (selected_point == alpha_control_pts.size() - 1) {
                        alpha_control_pts[selected_point].x = 1.f;
                    }
                } else {
                    auto fnd = std::find_if(
                        alpha_control_pts.begin(), alpha_control_pts.end(), [&](const vec2f &p) {
                            const vec2f pt_pos = p * view_scale + view_offset;
                            float dist = (pt_pos - vec2f(clipped_mouse_pos)).length();
                            return dist <= point_radius;
                        });
                    // No nearby point, we're adding a new one
                    if (fnd == alpha_control_pts.end()) {
                        alpha_control_pts.push_back(mouse_pos);
                    }
                }

                // Keep alpha control points ordered by x coordinate, update
                // selected point index to match
                std::sort(alpha_control_pts.begin(),
                          alpha_control_pts.end(),
                          [](const vec2f &a, const vec2f &b) { return a.x < b.x; });
                if (selected_point != 0 && selected_point != alpha_control_pts.size() - 1) {
                    auto fnd = std::find_if(
                        alpha_control_pts.begin(), alpha_control_pts.end(), [&](const vec2f &p) {
                            const vec2f pt_pos = p * view_scale + view_offset;
                            float dist = (pt_pos - vec2f(clipped_mouse_pos)).length();
                            return dist <= point_radius;
                        });
                    selected_point = std::distance(alpha_control_pts.begin(), fnd);
                }
                update_colormap();
            } else if (ImGui::IsMouseClicked(1)) {
                selected_point = SIZE_MAX;
                // Find and remove the point
                auto fnd = std::find_if(
                    alpha_control_pts.begin(), alpha_control_pts.end(), [&](const vec2f &p) {
                        const vec2f pt_pos = p * view_scale + view_offset;
                        float dist = (pt_pos - vec2f(clipped_mouse_pos)).length();
                        return dist <= point_radius;
                    });
                // We also want to prevent erasing the first and last points
                if (fnd != alpha_control_pts.end() && fnd != alpha_control_pts.begin() &&
                    fnd != alpha_control_pts.end() - 1) {
                    alpha_control_pts.erase(fnd);
                }
                update_colormap();
            } else {
                selected_point = SIZE_MAX;
            }
        } else {
            selected_point = SIZE_MAX;
        }

        // Draw the alpha control points, and build the points for the polyline
        // which connects them
        std::vector<ImVec2> polyline_pts;
        for (const auto &pt : alpha_control_pts) {
            const vec2f pt_pos = pt * view_scale + view_offset;
            
            // 确保控制点在安全区域内
            if (pt_pos.x >= clip_min.x && pt_pos.x <= clip_max.x &&
                pt_pos.y >= clip_min.y && pt_pos.y <= clip_max.y) {
                polyline_pts.push_back(pt_pos);
                draw_list->AddCircleFilled(pt_pos, point_radius, 0xFFFFFFFF);
            }
        }
        
        if (polyline_pts.size() > 1) {
            draw_list->AddPolyline(
                polyline_pts.data(), (int)polyline_pts.size(), 0xFFFFFFFF, ImDrawFlags_None, 2.f);
        }
        
        draw_list->PopClipRect();
    } else {
        // 如果剪裁区域无效，显示警告信息
        ImGui::Text("Transfer Function area too small");
        ImGui::Text("Please resize the window or panel");
    }
}

bool WebGPUTransferFunctionWidget::changed() const
{
    return colormap_changed;
}

std::vector<uint8_t> WebGPUTransferFunctionWidget::get_colormap()
{
    colormap_changed = false;
    return current_colormap;
}

std::vector<float> WebGPUTransferFunctionWidget::get_colormapf()
{
    colormap_changed = false;
    std::vector<float> colormapf(current_colormap.size(), 0.f);
    for (size_t i = 0; i < current_colormap.size(); ++i) {
        colormapf[i] = current_colormap[i] / 255.f;
    }
    return colormapf;
}

void WebGPUTransferFunctionWidget::get_colormapf(std::vector<float> &color,
                                                 std::vector<float> &opacity)
{
    colormap_changed = false;
    color.resize((current_colormap.size() / 4) * 3);
    opacity.resize(current_colormap.size() / 4);
    for (size_t i = 0; i < current_colormap.size() / 4; ++i) {
        color[i * 3] = current_colormap[i * 4] / 255.f;
        color[i * 3 + 1] = current_colormap[i * 4 + 1] / 255.f;
        color[i * 3 + 2] = current_colormap[i * 4 + 2] / 255.f;
        opacity[i] = current_colormap[i * 4 + 3] / 255.f;
    }
}

WGPUTexture WebGPUTransferFunctionWidget::get_webgpu_texture() const
{
    return colormap_texture;
}

WGPUTextureView WebGPUTransferFunctionWidget::get_webgpu_texture_view() const
{
    return colormap_view;
}

WGPUSampler WebGPUTransferFunctionWidget::get_webgpu_sampler() const
{
    return colormap_sampler;
}

void WebGPUTransferFunctionWidget::update_webgpu_texture()
{
    if (texture_needs_update && colormap_texture) {
        texture_needs_update = false;
        
        // 确保数据大小正确
        if (current_colormap.size() != COLORMAP_WIDTH * 4) {
            std::cout << "Warning: Incorrect colormap size " << current_colormap.size() 
                      << ", resizing to " << (COLORMAP_WIDTH * 4) << std::endl;
            current_colormap.resize(COLORMAP_WIDTH * 4);
        }
        
        WGPUImageCopyTexture destination = {};
        destination.texture = colormap_texture;
        destination.mipLevel = 0;
        destination.origin.x = 0;
        destination.origin.y = 0;
        destination.origin.z = 0;
        destination.aspect = WGPUTextureAspect_All;
        
        WGPUTextureDataLayout data_layout = {};
        data_layout.offset = 0;
        data_layout.bytesPerRow = COLORMAP_WIDTH * 4; // 256 pixels * 4 bytes per pixel (RGBA)
        data_layout.rowsPerImage = 1;
        
        WGPUExtent3D write_size = {};
        write_size.width = COLORMAP_WIDTH;
        write_size.height = 1;
        write_size.depthOrArrayLayers = 1;
        
        // std::cout << "Writing texture: buffer size = " << current_colormap.size() 
        //           << ", expected size = " << (COLORMAP_WIDTH * 4) << std::endl;
        
        wgpuQueueWriteTexture(queue, &destination, current_colormap.data(), 
                              current_colormap.size(), &data_layout, &write_size);
    }
}

void WebGPUTransferFunctionWidget::update_colormap()
{
    colormap_changed = true;
    texture_needs_update = true;
    
    // 确保选中的颜色映射有效
    if (selected_colormap >= colormaps.size()) {
        selected_colormap = 0;
    }
    
    if (colormaps.empty()) {
        std::cout << "Warning: No colormaps available!" << std::endl;
        return;
    }
    
    // 复制选中的颜色映射
    current_colormap = colormaps[selected_colormap].colormap;
    
    // 确保 current_colormap 有正确的大小
    if (current_colormap.size() != COLORMAP_WIDTH * 4) {
        std::cout << "Resizing colormap from " << current_colormap.size() 
                  << " to " << (COLORMAP_WIDTH * 4) << " bytes" << std::endl;
        
        std::vector<uint8_t> old_colormap = current_colormap;
        current_colormap.resize(COLORMAP_WIDTH * 4);
        
        // 如果原始颜色映射太小，通过插值填充
        if (old_colormap.size() >= 4) {
            size_t old_width = old_colormap.size() / 4;
            for (size_t i = 0; i < COLORMAP_WIDTH; ++i) {
                float t = static_cast<float>(i) / (COLORMAP_WIDTH - 1);
                float old_idx_f = t * (old_width - 1);
                size_t old_idx0 = static_cast<size_t>(old_idx_f);
                size_t old_idx1 = std::min(old_idx0 + 1, old_width - 1);
                float frac = old_idx_f - old_idx0;
                
                for (int c = 0; c < 4; ++c) {
                    float val0 = old_colormap[old_idx0 * 4 + c] / 255.0f;
                    float val1 = old_colormap[old_idx1 * 4 + c] / 255.0f;
                    float interpolated = val0 + frac * (val1 - val0);
                    current_colormap[i * 4 + c] = static_cast<uint8_t>(clamp(interpolated * 255.0f, 0.0f, 255.0f));
                }
            }
        } else {
            // 如果没有有效数据，创建默认渐变
            for (size_t i = 0; i < COLORMAP_WIDTH; ++i) {
                float t = static_cast<float>(i) / (COLORMAP_WIDTH - 1);
                current_colormap[i * 4 + 0] = static_cast<uint8_t>(255 * t); // R
                current_colormap[i * 4 + 1] = static_cast<uint8_t>(255 * t); // G
                current_colormap[i * 4 + 2] = static_cast<uint8_t>(255 * t); // B
                current_colormap[i * 4 + 3] = 255; // A
            }
        }
    }
    
    // 更新透明度通道基于控制点
    auto a_it = alpha_control_pts.begin();
    const size_t npixels = COLORMAP_WIDTH;
    for (size_t i = 0; i < npixels; ++i) {
        float x = static_cast<float>(i) / (npixels - 1);
        
        // 找到合适的控制点区间
        while (a_it + 1 != alpha_control_pts.end() && (a_it + 1)->x < x) {
            ++a_it;
        }
        
        auto high = a_it + 1;
        if (high == alpha_control_pts.end()) {
            high = a_it;
        }
        
        float alpha;
        if (a_it == high || std::abs(high->x - a_it->x) < 1e-6f) {
            alpha = a_it->y;
        } else {
            float t = (x - a_it->x) / (high->x - a_it->x);
            alpha = (1.f - t) * a_it->y + t * high->y;
        }
        
        current_colormap[i * 4 + 3] = static_cast<uint8_t>(clamp(alpha * 255.f, 0.f, 255.f));
    }
    
    std::cout << "Updated colormap: " << colormaps[selected_colormap].name 
              << ", final size: " << current_colormap.size() << " bytes" << std::endl;
}

void WebGPUTransferFunctionWidget::load_embedded_preset(const uint8_t *buf,
                                                        size_t size,
                                                        const std::string &name)
{
    int w, h, n;
    uint8_t *img_data = stbi_load_from_memory(buf, (int)size, &w, &h, &n, 4);
    if (!img_data) {
        std::cout << "Warning: Failed to load embedded preset: " << name << std::endl;
        return;
    }
    
    auto img = std::vector<uint8_t>(img_data, img_data + w * h * 4);
    stbi_image_free(img_data);
    
    // 确保图像是 1D 的（高度为 1）
    if (h != 1) {
        std::cout << "Warning: Embedded preset " << name << " is not 1D (height=" << h << "), using first row only" << std::endl;
        std::vector<uint8_t> first_row(img.begin(), img.begin() + w * 4);
        img = first_row;
    }
    
    colormaps.emplace_back(name, img, SRGB);
    Colormap &cmap = colormaps.back();
    
    // 转换 sRGB 到线性
    for (size_t i = 0; i < cmap.colormap.size() / 4; ++i) {
        for (size_t j = 0; j < 3; ++j) {
            const float x = srgb_to_linear(cmap.colormap[i * 4 + j] / 255.f);
            cmap.colormap[i * 4 + j] = static_cast<uint8_t>(clamp(x * 255.f, 0.f, 255.f));
        }
    }
    
    std::cout << "Loaded embedded preset: " << name << " (" << w << "x" << h << " pixels, " 
              << cmap.colormap.size() << " bytes)" << std::endl;
}

}