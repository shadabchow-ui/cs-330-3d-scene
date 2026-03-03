///////////////////////////////////////////////////////////////////////////////
// SceneManager.h
// ============
// Manage preparation and rendering of 3D scenes (meshes, textures, materials,
// lighting, and object placement).
//
// AUTHOR: Brian Battersby - SNHU Instructor / Computer Science
// Created for CS-330-Computational Graphics and Visualization
//
// Student modifications:
// - Added texture loading support (CreateGLTexture / BindGLTextures)
// - Added OBJECT_TEXTURE struct for texture tag/ID bookkeeping
// - Added full material/lighting/scene rendering declarations
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include "ShaderManager.h"
#include "ShapeMeshes.h"

// GLEW / GLFW / GLM
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

#include <string>
#include <vector>

/***********************************************************
 * OBJECT_MATERIAL
 * Simple container for Phong material properties.
 ***********************************************************/
struct OBJECT_MATERIAL {
    std::string tag;
    glm::vec3 diffuseColor = glm::vec3(1.0f);
    glm::vec3 specularColor = glm::vec3(1.0f);
    float     shininess = 32.0f;
};

/***********************************************************
 * OBJECT_TEXTURE
 * Maps a string tag to an OpenGL texture ID so we can
 * refer to textures by name when rendering.
 ***********************************************************/
struct OBJECT_TEXTURE {
    std::string tag;
    GLuint      ID = 0;
};

/***********************************************************
 * SceneManager
 ***********************************************************/
class SceneManager {
public:
    // constructor / destructor
    SceneManager(ShaderManager* pShaderManager);
    ~SceneManager();

    // called once at startup
    void PrepareScene();

    // called every frame
    void RenderScene();

private:
    // ----- managers / meshes -----
    ShaderManager* m_pShaderManager = nullptr;
    ShapeMeshes* m_basicMeshes = nullptr;

    // ----- materials -----
    std::vector<OBJECT_MATERIAL> m_objectMaterials;
    bool FindMaterial(std::string tag, OBJECT_MATERIAL& material);
    void DefineObjectMaterials();
    void SetShaderMaterial(std::string materialTag);

    // ----- textures -----
    std::vector<OBJECT_TEXTURE> m_objectTextures;
    bool CreateGLTexture(const char* filename, std::string tag);
    void BindGLTextures();
    void DestroyGLTextures();
    void SetShaderTexture(std::string textureTag);
    void SetTextureUVScale(float u, float v);

    // ----- lighting -----
    void SetupSceneLights();

    // ----- transform / shader helpers -----
    void SetTransformations(glm::vec3 scaleXYZ,
        float XrotationDegrees,
        float YrotationDegrees,
        float ZrotationDegrees,
        glm::vec3 positionXYZ);
    void SetShaderColor(float r, float g, float b, float a);
};