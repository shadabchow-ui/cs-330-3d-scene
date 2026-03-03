///////////////////////////////////////////////////////////////////////////////
// ViewManager.cpp
// ============
// manage the viewing of 3D objects within the viewport - camera, projection
//
//  AUTHOR: Brian Battersby - SNHU Instructor / Computer Science
//  Created for CS-330-Computational Graphics and Visualization, Nov. 1st, 2023
//
//  Student modifications:
//  - Added Q/E vertical movement
//  - Added mouse scroll callback for movement speed adjustment
//  - Added perspective/orthographic toggle with P key
///////////////////////////////////////////////////////////////////////////////

#include "ViewManager.h"

// GLM Math Header inclusions
#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <algorithm>
#include <iostream>

// declarations for global variables and defines
namespace
{
	// Variables for window width and height
	const int WINDOW_WIDTH = 1000;
	const int WINDOW_HEIGHT = 800;

	const char* g_ViewName = "view";
	const char* g_ProjectionName = "projection";

	// camera object used for viewing and interacting with the 3D scene
	Camera* g_pCamera = nullptr;

	// mouse movement processing
	float gLastX = WINDOW_WIDTH / 2.0f;
	float gLastY = WINDOW_HEIGHT / 2.0f;
	bool gFirstMouse = true;

	// time between current frame and last frame
	float gDeltaTime = 0.0f;
	float gLastFrame = 0.0f;

	// projection mode
	bool gUseOrthographicProjection = false;

	// key debounce for projection toggle
	bool gProjectionTogglePressed = false;

	// mouse wheel behavior: use scroll to adjust movement speed
	// (Rubric allows speed OR zoom, but must match implementation)
	float gMinMoveSpeed = 2.0f;
	float gMaxMoveSpeed = 80.0f;
	float gScrollSpeedStep = 1.5f;
}

/***********************************************************
 *  ViewManager()
 *
 *  The constructor for the class
 ***********************************************************/
ViewManager::ViewManager(ShaderManager* pShaderManager)
{
	// initialize the member variables
	m_pShaderManager = pShaderManager;
	m_pWindow = NULL;

	g_pCamera = new Camera();

	// Default camera view parameters for the workstation scene
	// Positioned back and above so the whole desk setup is visible.
	g_pCamera->Position = glm::vec3(0.0f, 6.5f, 16.0f);

	// IMPORTANT:
	// Front should be a direction vector, not a point in space.
	// Slight downward angle toward the desk.
	g_pCamera->Front = glm::normalize(glm::vec3(0.0f, -0.30f, -1.0f));
	g_pCamera->Up = glm::vec3(0.0f, 1.0f, 0.0f);

	// Wider field of view helps frame the desk and walls
	g_pCamera->Zoom = 60.0f;

	// Scroll wheel will modify this at runtime
	g_pCamera->MovementSpeed = 12.0f;
}

/***********************************************************
 *  ~ViewManager()
 *
 *  The destructor for the class
 ***********************************************************/
ViewManager::~ViewManager()
{
	// free up allocated memory
	m_pShaderManager = NULL;
	m_pWindow = NULL;

	if (NULL != g_pCamera)
	{
		delete g_pCamera;
		g_pCamera = NULL;
	}
}

/***********************************************************
 *  CreateDisplayWindow()
 *
 *  This method is used to create the main display window.
 ***********************************************************/
GLFWwindow* ViewManager::CreateDisplayWindow(const char* windowTitle)
{
	GLFWwindow* window = nullptr;

	// try to create the displayed OpenGL window
	window = glfwCreateWindow(
		WINDOW_WIDTH,
		WINDOW_HEIGHT,
		windowTitle,
		NULL, NULL);

	if (window == NULL)
	{
		std::cout << "Failed to create GLFW window" << std::endl;
		glfwTerminate();
		return NULL;
	}

	glfwMakeContextCurrent(window);

	// callbacks for mouse input
	glfwSetCursorPosCallback(window, &ViewManager::Mouse_Position_Callback);
	glfwSetScrollCallback(window, &ViewManager::Mouse_Scroll_Callback);

	// tell GLFW to capture all mouse events
	glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

	// enable blending for supporting transparent rendering
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	m_pWindow = window;
	return window;
}

/***********************************************************
 *  Mouse_Position_Callback()
 *
 *  This method is automatically called from GLFW whenever
 *  the mouse is moved within the active GLFW display window.
 ***********************************************************/
void ViewManager::Mouse_Position_Callback(GLFWwindow* window, double xMousePos, double yMousePos)
{
	(void)window; // unused in this callback

	// record first mouse movement to prevent jump
	if (gFirstMouse)
	{
		gLastX = static_cast<float>(xMousePos);
		gLastY = static_cast<float>(yMousePos);
		gFirstMouse = false;
	}

	// calculate the X offset and Y offset values for moving the 3D camera accordingly
	float xOffset = static_cast<float>(xMousePos) - gLastX;
	float yOffset = gLastY - static_cast<float>(yMousePos); // reversed since y-coordinates go bottom->top

	// set current positions into the last position variables
	gLastX = static_cast<float>(xMousePos);
	gLastY = static_cast<float>(yMousePos);

	// move the 3D camera according to the calculated offsets
	if (g_pCamera != nullptr)
	{
		g_pCamera->ProcessMouseMovement(xOffset, yOffset);
	}
}

/***********************************************************
 *  Mouse_Scroll_Callback()
 *
 *  Uses mouse wheel to adjust camera movement speed
 *  (acceptable per rubric: speed OR zoom).
 ***********************************************************/
void ViewManager::Mouse_Scroll_Callback(GLFWwindow* window, double xOffset, double yOffset)
{
	(void)window;   // unused
	(void)xOffset;  // unused

	if (g_pCamera == nullptr)
	{
		return;
	}

	// Increase / decrease movement speed using scroll wheel
	float newSpeed = g_pCamera->MovementSpeed + static_cast<float>(yOffset) * gScrollSpeedStep;
	newSpeed = std::max(gMinMoveSpeed, std::min(gMaxMoveSpeed, newSpeed));
	g_pCamera->MovementSpeed = newSpeed;

	// OPTIONAL: If you want scroll to control zoom instead, use this instead:
	// g_pCamera->ProcessMouseScroll(static_cast<float>(yOffset));
}

/***********************************************************
 *  ProcessKeyboardEvents()
 *
 *  This method is called to process any keyboard events
 *  that may be waiting in the event queue.
 ***********************************************************/
void ViewManager::ProcessKeyboardEvents()
{
	if (m_pWindow == nullptr || g_pCamera == nullptr)
	{
		return;
	}

	// close the window if the escape key has been pressed
	if (glfwGetKey(m_pWindow, GLFW_KEY_ESCAPE) == GLFW_PRESS)
	{
		glfwSetWindowShouldClose(m_pWindow, true);
	}

	// WASD movement
	if (glfwGetKey(m_pWindow, GLFW_KEY_W) == GLFW_PRESS)
	{
		g_pCamera->ProcessKeyboard(FORWARD, gDeltaTime);
	}
	if (glfwGetKey(m_pWindow, GLFW_KEY_S) == GLFW_PRESS)
	{
		g_pCamera->ProcessKeyboard(BACKWARD, gDeltaTime);
	}
	if (glfwGetKey(m_pWindow, GLFW_KEY_A) == GLFW_PRESS)
	{
		g_pCamera->ProcessKeyboard(LEFT, gDeltaTime);
	}
	if (glfwGetKey(m_pWindow, GLFW_KEY_D) == GLFW_PRESS)
	{
		g_pCamera->ProcessKeyboard(RIGHT, gDeltaTime);
	}

	// Q/E vertical movement
	// Requested by rubric:
	// Q/E keys should control upward and downward movement.
	// (We'll implement Q = down, E = up to match your note.)
	if (glfwGetKey(m_pWindow, GLFW_KEY_Q) == GLFW_PRESS)
	{
		g_pCamera->ProcessKeyboard(DOWN, gDeltaTime);
	}
	if (glfwGetKey(m_pWindow, GLFW_KEY_E) == GLFW_PRESS)
	{
		g_pCamera->ProcessKeyboard(UP, gDeltaTime);
	}

	// Projection toggle: P key toggles perspective/orthographic
	if (glfwGetKey(m_pWindow, GLFW_KEY_P) == GLFW_PRESS)
	{
		if (!gProjectionTogglePressed)
		{
			gUseOrthographicProjection = !gUseOrthographicProjection;
			gProjectionTogglePressed = true;
		}
	}
	else
	{
		gProjectionTogglePressed = false;
	}
}

/***********************************************************
 *  PrepareSceneView()
 *
 *  This method is used for preparing the 3D scene by loading
 *  the shapes, textures in memory to support the 3D scene
 *  rendering
 ***********************************************************/
void ViewManager::PrepareSceneView()
{
	glm::mat4 view(1.0f);
	glm::mat4 projection(1.0f);

	// per-frame timing
	float currentFrame = static_cast<float>(glfwGetTime());
	gDeltaTime = currentFrame - gLastFrame;
	gLastFrame = currentFrame;

	// process input
	ProcessKeyboardEvents();

	// get current view matrix from camera
	view = g_pCamera->GetViewMatrix();

	// projection mode
	const float aspect = static_cast<float>(WINDOW_WIDTH) / static_cast<float>(WINDOW_HEIGHT);

	if (!gUseOrthographicProjection)
	{
		// Perspective projection
		projection = glm::perspective(
			glm::radians(g_pCamera->Zoom),
			aspect,
			0.1f,
			100.0f);
	}
	else
	{
		// Orthographic projection sized to fit the desk scene
		// Keep camera orientation the same; only change projection matrix.
		const float orthoScale = 10.0f;
		projection = glm::ortho(
			-orthoScale * aspect,
			orthoScale * aspect,
			-orthoScale,
			orthoScale,
			-50.0f,
			100.0f);
	}

	// send matrices + camera position to shader
	if (NULL != m_pShaderManager)
	{
		m_pShaderManager->setMat4Value(g_ViewName, view);
		m_pShaderManager->setMat4Value(g_ProjectionName, projection);
		m_pShaderManager->setVec3Value("viewPosition", g_pCamera->Position);
	}
}