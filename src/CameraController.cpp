#include "CameraController.h"
#include <iostream>

CameraController::CameraController(Camera* camera)
    : m_camera(camera) {}

void CameraController::OnMouseButton(int button, int action, int /*mods*/) {
    if (button == 0) // Left
        m_leftButtonPressed = (action == 1);
    else if (button == 1) // Right
        m_rightButtonPressed = (action == 1);
}

void CameraController::OnMouseMove(double xpos, double ypos) {
    glm::vec2 currentPos(xpos, ypos);
    glm::vec2 delta = currentPos - m_lastMousePos;
    m_lastMousePos = currentPos;

    auto mode = m_camera->GetCameraMode();
    if (m_leftButtonPressed) {
        if (mode == Camera::CameraMode::Ortho2D) {
            m_camera->Pan(-delta.x * 0.005f, delta.y * 0.005f);
        } else if (mode == Camera::CameraMode::Turntable3D) {
            m_camera->Rotate(delta.x * 0.3f, -delta.y * 0.3f);
        } else if (mode == Camera::CameraMode::Free3D) {
            m_camera->Rotate(delta.x * 0.3f, -delta.y * 0.3f);
        }
    }
}

void CameraController::OnMouseScroll([[maybe_unused]]double xoffset, double yoffset) {
    m_camera->Zoom(static_cast<float>(yoffset));
}

void CameraController::OnKeyPress(int key, int action) 
{
    float value = (action == 1 || action == 2) ? 1.0f : 0.0f;
    switch (key) 
    {
        case 'W': m_movementInput.y = -value; break;
        case 'S': m_movementInput.y = +value; break;
        case 'A': m_movementInput.x = -value; break;
        case 'D': m_movementInput.x = +value; break;
        case 'Q': m_movementInput.z = -value; break;
        case 'E': m_movementInput.z = +value; break;
    }
}

void CameraController::Update(float deltaTime) 
{
    if (m_camera->GetCameraMode() == Camera::CameraMode::Free3D) 
    {
        glm::vec3 move = m_movementInput * m_moveSpeed * deltaTime;
        m_camera->MoveFreeCamera(move);
    }
    else if (m_camera->GetCameraMode() == Camera::CameraMode::Ortho2D) 
    {
        // 2D 模式下的移动
        m_moveSpeed = 100.0f; // 可以根据需要调整速度
        glm::vec3 move = m_movementInput * m_moveSpeed * deltaTime;
        m_camera->Pan(-move.x, move.y);
    }
}