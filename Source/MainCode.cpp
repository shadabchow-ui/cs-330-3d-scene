///////////////////////////////////////////////////////////////////////////////
// MainCode.cpp
// ============
// gets called when application is launched - initializes GLEW, GLFW
//
//  AUTHOR: Brian Battersby - SNHU Instructor / Computer Science
//  Created for CS-330-Computational Graphics and Visualization, Nov. 1st, 2023
//
//  Student modifications:
//  - Final project integration (custom scene + camera controls)
//  - Supports perspective/orthographic toggle via ViewManager
///////////////////////////////////////////////////////////////////////////////

#include <iostream>
#include <cstdlib>

#include <GL/glew.h>
#include "GLFW/glfw3.h"

// GLM Math Header inclusions
#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "SceneManager.h"
#include "ViewManager.h"
#include "ShapeMeshes.h"
#include "ShaderManager.h"

// Namespace for declaring global variables
namespace
{
	// Window title
	const char* const WINDOW_TITLE = "CS-330 Final Project - Workstation Scene";

	// Main GLFW window
	GLFWwindow* g_Window = nullptr;

	// Managers
	SceneManager* g_SceneManager = nullptr;
	ShaderManager* g_ShaderManager = nullptr;
	ViewManager* g_ViewManager = nullptr;
}

// Function declarations
bool InitializeGLFW();
bool InitializeGLEW();

/***********************************************************
 *  main(int, char*)
 ***********************************************************/
int main(int argc, char* argv[])
{
	(void)argc;
	(void)argv;

	// Initialize GLFW
	if (!InitializeGLFW())
	{
		return EXIT_FAILURE;
	}

	// Create managers
	g_ShaderManager = new ShaderManager();
	g_ViewManager = new ViewManager(g_ShaderManager);

	// Create display window
	g_Window = g_ViewManager->CreateDisplayWindow(WINDOW_TITLE);
	if (g_Window == nullptr)
	{
		return EXIT_FAILURE;
	}

	// Initialize GLEW
	if (!InitializeGLEW())
	{
		return EXIT_FAILURE;
	}

	// Load shaders
	g_ShaderManager->LoadShaders(
		"../../Utilities/shaders/vertexShader.glsl",
		"../../Utilities/shaders/fragmentShader.glsl");
	g_ShaderManager->use();

	// Prepare scene
	g_SceneManager = new SceneManager(g_ShaderManager);
	g_SceneManager->PrepareScene();

	// Render loop
	while (!glfwWindowShouldClose(g_Window))
	{
		// Enable depth testing
		glEnable(GL_DEPTH_TEST);

		// Clear buffers
		glClearColor(0.03f, 0.03f, 0.04f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		// Prepare camera view + projection
		g_ViewManager->PrepareSceneView();

		// Render scene
		g_SceneManager->RenderScene();

		// Present frame
		glfwSwapBuffers(g_Window);

		// Poll events
		glfwPollEvents();
	}

	// Cleanup
	if (g_SceneManager != NULL)
	{
		delete g_SceneManager;
		g_SceneManager = NULL;
	}
	if (g_ViewManager != NULL)
	{
		delete g_ViewManager;
		g_ViewManager = NULL;
	}
	if (g_ShaderManager != NULL)
	{
		delete g_ShaderManager;
		g_ShaderManager = NULL;
	}

	glfwTerminate();
	return EXIT_SUCCESS;
}

/***********************************************************
 *  InitializeGLFW()
 ***********************************************************/
bool InitializeGLFW()
{
	glfwInit();

#ifdef __APPLE__
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#else
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#endif

	return true;
}

/***********************************************************
 *  InitializeGLEW()
 ***********************************************************/
bool InitializeGLEW()
{
	GLenum GLEWInitResult = GLEW_OK;

	GLEWInitResult = glewInit();
	if (GLEW_OK != GLEWInitResult)
	{
		std::cerr << glewGetErrorString(GLEWInitResult) << std::endl;
		return false;
	}

	std::cout << "INFO: OpenGL Successfully Initialized\n";
	std::cout << "INFO: OpenGL Version: " << glGetString(GL_VERSION) << "\n" << std::endl;

	return true;
}