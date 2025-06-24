#pragma once
#include "ggl.h"
#include "Camera.hpp"


class CameraController 
{
public:
    CameraController(Camera* camera);

    void OnMouseButton(int button, int action, int mods);
    void OnMouseMove(double xpos, double ypos);
    void OnMouseScroll(double xoffset, double yoffset);
    void OnKeyPress(int key, int action);

    void Update(float deltaTime);  // For Free3D movement over time
    Camera* GetCamera() const { return m_camera; }
private:
    Camera* m_camera;
    bool m_leftButtonPressed = false;
    bool m_rightButtonPressed = false;
    glm::vec2 m_lastMousePos;

    // For Free3D
    glm::vec3 m_movementInput = glm::vec3(0.0f);
    float m_moveSpeed = 5.0f;
};