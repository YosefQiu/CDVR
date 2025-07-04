#pragma once
#include "ggl.h"

class Camera 
{
public:
    enum class ProjectionType {
        Orthographic,
        Perspective
    };

    enum class CameraMode {
        Ortho2D,
        Turntable3D,
        Free3D
    };

    Camera(CameraMode mode = CameraMode::Ortho2D)
        : m_mode(mode),
          m_type(mode == CameraMode::Ortho2D ? ProjectionType::Orthographic : ProjectionType::Perspective),
          m_position(0.0f, 0.0f, 5.0f),
          m_target(0.0f, 0.0f, 0.0f),
          m_up(0.0f, 1.0f, 0.0f),
          m_yaw(0.0f), m_pitch(0.0f), m_radius(5.0f),
          m_fovY(45.0f), m_near(0.1f), m_far(100.0f),
          m_orthoLeft(-1.0f), m_orthoRight(1.0f),
          m_orthoBottom(-1.0f), m_orthoTop(1.0f),
          m_viewportWidth(800), m_viewportHeight(600) {}

    void SetViewportSize(int width, int height) {
        m_viewportWidth = width;
        m_viewportHeight = height;
    }

    void SetPosition(const glm::vec3& pos) { m_position = pos; }
    void SetTarget(const glm::vec3& target) { m_target = target; }
    void SetUp(const glm::vec3& up) { m_up = up; }
    void SetYawPitch(float yaw, float pitch) { m_yaw = yaw; m_pitch = pitch; }

    void Zoom(float delta) 
    {
        if (m_mode == CameraMode::Ortho2D) 
        {
            // 2D 模式缩放
            float zoomFactor = std::pow(1.1f, -delta);  // 控制缩放速度

            float cx = (m_orthoLeft + m_orthoRight) * 0.5f;
            float cy = (m_orthoBottom + m_orthoTop) * 0.5f;
            float width = (m_orthoRight - m_orthoLeft) * zoomFactor;
            float height = (m_orthoTop - m_orthoBottom) * zoomFactor;

            m_orthoLeft   = cx - width * 0.5f;
            m_orthoRight  = cx + width * 0.5f;
            m_orthoBottom = cy - height * 0.5f;
            m_orthoTop    = cy + height * 0.5f;
        } 
        else 
        {
            m_radius -= delta;
            m_radius = glm::clamp(m_radius, 1.0f, 100.0f);
            m_fovY -= delta;
            m_fovY = glm::clamp(m_fovY, 10.0f, 90.0f);
        }
    }

    void Pan(float dx, float dy) {
        glm::vec3 right = glm::normalize(glm::cross(GetForward(), m_up));
        glm::vec3 upMove = glm::normalize(m_up);
        m_position += dx * right + dy * upMove;
        m_target += dx * right + dy * upMove;
    }

    void Rotate(float yawOffset, float pitchOffset) {
        m_yaw += yawOffset;
        m_pitch += pitchOffset;
        m_pitch = glm::clamp(m_pitch, -89.0f, 89.0f);
    }

    void MoveFreeCamera(const glm::vec3& delta) {
        if (m_mode == CameraMode::Free3D) {
            glm::vec3 forward = GetForward();
            glm::vec3 right = glm::normalize(glm::cross(forward, m_up));
            m_position += delta.x * right + delta.y * m_up + delta.z * forward;
        }
    }
    glm::vec3 GetPosition() const { return m_position; }
    glm::vec3 GetTarget() const { return m_target; }
    glm::vec3 GetUp() const { return m_up; }
    glm::mat4 GetViewMatrix() const {
        if (m_mode == CameraMode::Ortho2D) {
            // 2D 模式的视图矩阵
            // return glm::mat4(1.0f); 
            // return glm::translate(glm::mat4(1.0f), -glm::vec3(m_position.x, m_position.y, 0.0f));
            return glm::lookAt(m_position, m_target, m_up);
        }
        else if (m_mode == CameraMode::Turntable3D) {
            glm::vec3 offset;
            offset.x = m_radius * cos(glm::radians(m_pitch)) * sin(glm::radians(m_yaw));
            offset.y = m_radius * sin(glm::radians(m_pitch));
            offset.z = m_radius * cos(glm::radians(m_pitch)) * cos(glm::radians(m_yaw));
            glm::vec3 camPos = m_target + offset;
            return glm::lookAt(camPos, m_target, m_up);
        } else if (m_mode == CameraMode::Free3D) {
            glm::vec3 front;
            front.x = cos(glm::radians(m_yaw)) * cos(glm::radians(m_pitch));
            front.y = sin(glm::radians(m_pitch));
            front.z = sin(glm::radians(m_yaw)) * cos(glm::radians(m_pitch));
            return glm::lookAt(m_position, m_position + glm::normalize(front), m_up);
        } else {
            return glm::lookAt(m_position, m_target, m_up);
        }
    }

    glm::mat4 GetProjMatrix() const 
    {
        if (m_type == ProjectionType::Orthographic) {
            // 对于 2D 模式，使用 -1 到 1 的深度范围
            if (m_mode == CameraMode::Ortho2D) {
                return glm::ortho(m_orthoLeft, m_orthoRight, m_orthoBottom, m_orthoTop, -1.0f, 1.0f);
            } else {
                // 3D 正交投影使用正常的 near/far
                return glm::ortho(m_orthoLeft, m_orthoRight, m_orthoBottom, m_orthoTop, m_near, m_far);
            }
        } else {
            float aspect = static_cast<float>(m_viewportWidth) / m_viewportHeight;
            return glm::perspective(glm::radians(m_fovY), aspect, m_near, m_far);
        }
    }

    void SetOrtho(float left, float right, float bottom, float top) {
        m_orthoLeft = left;
        m_orthoRight = right;
        m_orthoBottom = bottom;
        m_orthoTop = top;

        UpdateProjectionMatrix();
    }

    void SetOrthoToFitContent(float contentWidth, float contentHeight, float windowAspect) 
    {
        float contentAspect = contentWidth / contentHeight;

        float left, right, bottom, top;

        if (windowAspect > contentAspect) {
            // 窗口更宽，横向留白
            float halfW = (contentHeight * windowAspect) / 2.0f;
            left = contentWidth / 2.0f - halfW;
            right = contentWidth / 2.0f + halfW;
            bottom = 0.0f;
            top = contentHeight;
        } else {
            // 窗口更高，纵向留白
            float halfH = (contentWidth / windowAspect) / 2.0f;
            left = 0.0f;
            right = contentWidth;
            bottom = contentHeight / 2.0f - halfH;
            top = contentHeight / 2.0f + halfH;
        }

        // 注意：Y轴翻转（bottom/top顺序）匹配 WebGPU NDC
        SetOrtho(left, right, bottom, top);
    }

    void SetPerspective(float fovYDegrees, float near, float far) {
        m_fovY = fovYDegrees;
        m_near = near;
        m_far = far;

        UpdateProjectionMatrix();
    }

    void SetCameraMode(CameraMode mode) {
        m_mode = mode;
        m_type = (mode == CameraMode::Ortho2D ? ProjectionType::Orthographic : ProjectionType::Perspective);
    }

    void UpdateProjectionMatrix() {
        if (m_mode == CameraMode::Ortho2D) {
            m_projMatrix = glm::ortho(m_orthoLeft, m_orthoRight, m_orthoBottom, m_orthoTop, m_near, m_far);
        }
    }

    CameraMode GetCameraMode() const { return m_mode; }
    ProjectionType GetProjectionType() const { return m_type; }

private:
    CameraMode m_mode;
    ProjectionType m_type;

    glm::vec3 m_position;
    glm::vec3 m_target;
    glm::vec3 m_up;

    float m_yaw, m_pitch;
    float m_radius;
    float m_fovY;
    float m_near, m_far;
    float m_orthoLeft, m_orthoRight, m_orthoBottom, m_orthoTop;
    glm::mat4 m_projMatrix = glm::mat4(1.0f); // Projection matrix
    glm::mat4 m_viewMatrix = glm::mat4(1.0f); // View matrix
    int m_viewportWidth, m_viewportHeight;

    glm::vec3 GetForward() const { return glm::normalize(m_target - m_position); }
};
