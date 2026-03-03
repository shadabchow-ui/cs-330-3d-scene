///////////////////////////////////////////////////////////////////////////////
// SceneManager.cpp
// ============
// Manage preparation and rendering of 3D scenes (meshes, textures, materials,
// lighting, and object placement).
//
// AUTHOR: Brian Battersby - SNHU Instructor / Computer Science
// Created for CS-330-Computational Graphics and Visualization
//
// Student modifications (final project):
// - Built desk workstation scene to match selected reference image
// - Added custom materials and multi-light Phong setup (including tinted light)
// - Integrated texture loading via stb_image (CreateGLTexture / BindGLTextures)
// - Applied textures to desk surface and back wall (rubric: >= 2 textured objects)
// - Helper lambdas in RenderScene for cleaner, modular draw calls
///////////////////////////////////////////////////////////////////////////////

#include "SceneManager.h"

// stb_image for texture loading
// NOTE: This #define must appear in EXACTLY ONE .cpp file in the project.
// If your project already has stb_image included elsewhere (e.g. in a
// TextureManager.cpp), remove this block and use that instead.
#ifndef STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#endif

#include <GL/glew.h>
#include <glm/gtx/transform.hpp>
#include <iostream>

// -----------------------------------------------------------------------------
// Shader uniform names used by the SNHU template shaders
// -----------------------------------------------------------------------------
namespace {
    const char* g_ModelName = "model";
    const char* g_ColorValueName = "objectColor";
    const char* g_TextureValueName = "objectTexture";
    const char* g_UseTextureName = "bUseTexture";
    const char* g_UseLightingName = "bUseLighting";
    const char* g_UVScaleName = "UVscale";
} // namespace

// =============================================================================
//  CONSTRUCTOR / DESTRUCTOR
// =============================================================================

SceneManager::SceneManager(ShaderManager* pShaderManager) {
    m_pShaderManager = pShaderManager;
    m_basicMeshes = new ShapeMeshes();
}

SceneManager::~SceneManager() {
    m_pShaderManager = NULL;

    if (NULL != m_basicMeshes) {
        delete m_basicMeshes;
        m_basicMeshes = NULL;
    }

    m_objectMaterials.clear();
    DestroyGLTextures();
}

// =============================================================================
//  TEXTURE LOADING (stb_image)
// =============================================================================

/***********************************************************
 *  CreateGLTexture()
 *
 *  Load an image file from disk, create an OpenGL texture,
 *  and store the tag + ID for later binding.
 *  Returns true on success.
 ***********************************************************/
bool SceneManager::CreateGLTexture(const char* filename, std::string tag) {
    int width = 0;
    int height = 0;
    int channels = 0;

    // stb_image loads with (0,0) at top-left; OpenGL expects bottom-left
    stbi_set_flip_vertically_on_load(true);

    unsigned char* image = stbi_load(filename, &width, &height, &channels, 0);
    if (image == nullptr) {
        std::cout << "ERROR: Could not load texture file: " << filename << std::endl;
        return false;
    }

    // Determine format based on channel count
    GLenum format = GL_RGB;
    if (channels == 1)      format = GL_RED;
    else if (channels == 3) format = GL_RGB;
    else if (channels == 4) format = GL_RGBA;

    GLuint textureID = 0;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);

    // Upload pixel data
    glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0,
        format, GL_UNSIGNED_BYTE, image);

    // Generate mipmaps for better quality at distance
    glGenerateMipmap(GL_TEXTURE_2D);

    // Texture filtering parameters
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // Free CPU-side image data
    stbi_image_free(image);

    // Store for later binding
    OBJECT_TEXTURE texInfo;
    texInfo.tag = tag;
    texInfo.ID = textureID;
    m_objectTextures.push_back(texInfo);

    std::cout << "INFO: Loaded texture \"" << tag
        << "\" from " << filename
        << " (" << width << "x" << height
        << ", " << channels << " ch)" << std::endl;

    return true;
}

/***********************************************************
 *  BindGLTextures()
 *
 *  Bind each loaded texture to a sequential texture unit
 *  (GL_TEXTURE0, GL_TEXTURE1, ...) so the shader can
 *  access them by sampler index.
 ***********************************************************/
void SceneManager::BindGLTextures() {
    for (int i = 0; i < (int)m_objectTextures.size(); i++) {
        glActiveTexture(GL_TEXTURE0 + i);
        glBindTexture(GL_TEXTURE_2D, m_objectTextures[i].ID);
    }
}

/***********************************************************
 *  DestroyGLTextures()
 *
 *  Delete all OpenGL texture objects we created.
 ***********************************************************/
void SceneManager::DestroyGLTextures() {
    for (int i = 0; i < (int)m_objectTextures.size(); i++) {
        glDeleteTextures(1, &m_objectTextures[i].ID);
    }
    m_objectTextures.clear();
}

/***********************************************************
 *  SetShaderTexture()
 *
 *  Find the texture with the given tag and tell the shader
 *  to use it by setting the sampler uniform to the matching
 *  texture-unit index.
 ***********************************************************/
void SceneManager::SetShaderTexture(std::string textureTag) {
    if (NULL == m_pShaderManager) return;

    for (int i = 0; i < (int)m_objectTextures.size(); i++) {
        if (m_objectTextures[i].tag == textureTag) {
            glActiveTexture(GL_TEXTURE0 + i);
            glBindTexture(GL_TEXTURE_2D, m_objectTextures[i].ID);
            m_pShaderManager->setIntValue(g_TextureValueName, i);
            return;
        }
    }
    // If tag not found, just leave current binding as-is
}

/***********************************************************
 *  SetTextureUVScale()
 *
 *  Set the UV tiling scale uniform so textures can tile
 *  across large surfaces.  The SNHU template shader uses
 *  "UVscale" as a vec2.
 ***********************************************************/
void SceneManager::SetTextureUVScale(float u, float v) {
    if (NULL != m_pShaderManager) {
        m_pShaderManager->setVec2Value(g_UVScaleName, glm::vec2(u, v));
    }
}

// =============================================================================
//  MATERIAL LOOKUP
// =============================================================================

bool SceneManager::FindMaterial(std::string tag, OBJECT_MATERIAL& material) {
    if (m_objectMaterials.size() == 0) return false;

    int  index = 0;
    bool bFound = false;

    while ((index < (int)m_objectMaterials.size()) && (!bFound)) {
        if (m_objectMaterials[index].tag == tag) {
            bFound = true;
            material.diffuseColor = m_objectMaterials[index].diffuseColor;
            material.specularColor = m_objectMaterials[index].specularColor;
            material.shininess = m_objectMaterials[index].shininess;
        }
        else {
            index++;
        }
    }

    return bFound;
}

// =============================================================================
//  SHADER HELPERS
// =============================================================================

/***********************************************************
 *  SetTransformations()
 *  Build model matrix: Scale -> Rotate X/Y/Z -> Translate
 ***********************************************************/
void SceneManager::SetTransformations(glm::vec3 scaleXYZ,
    float XrotationDegrees,
    float YrotationDegrees,
    float ZrotationDegrees,
    glm::vec3 positionXYZ) {
    glm::mat4 scale = glm::scale(scaleXYZ);
    glm::mat4 rotationX = glm::rotate(glm::radians(XrotationDegrees), glm::vec3(1.0f, 0.0f, 0.0f));
    glm::mat4 rotationY = glm::rotate(glm::radians(YrotationDegrees), glm::vec3(0.0f, 1.0f, 0.0f));
    glm::mat4 rotationZ = glm::rotate(glm::radians(ZrotationDegrees), glm::vec3(0.0f, 0.0f, 1.0f));
    glm::mat4 translation = glm::translate(positionXYZ);

    glm::mat4 modelView = translation * rotationZ * rotationY * rotationX * scale;

    if (NULL != m_pShaderManager) {
        m_pShaderManager->setMat4Value(g_ModelName, modelView);
    }
}

/***********************************************************
 *  SetShaderColor()
 ***********************************************************/
void SceneManager::SetShaderColor(float redColorValue, float greenColorValue,
    float blueColorValue, float alphaValue) {
    glm::vec4 currentColor(redColorValue, greenColorValue, blueColorValue, alphaValue);

    if (NULL != m_pShaderManager) {
        m_pShaderManager->setVec4Value(g_ColorValueName, currentColor);
    }
}

/***********************************************************
 *  SetShaderMaterial()
 ***********************************************************/
void SceneManager::SetShaderMaterial(std::string materialTag) {
    if ((NULL == m_pShaderManager) || (m_objectMaterials.size() == 0))
        return;

    OBJECT_MATERIAL material;
    if (FindMaterial(materialTag, material)) {
        m_pShaderManager->setVec3Value("material.diffuseColor", material.diffuseColor);
        m_pShaderManager->setVec3Value("material.specularColor", material.specularColor);
        m_pShaderManager->setFloatValue("material.shininess", material.shininess);
    }
}

// =============================================================================
//  MATERIAL DEFINITIONS
// =============================================================================

void SceneManager::DefineObjectMaterials() {
    m_objectMaterials.clear();
    OBJECT_MATERIAL material;

    // Neutral wall
    material.tag = "matteWall";
    material.diffuseColor = glm::vec3(0.90f, 0.89f, 0.84f);
    material.specularColor = glm::vec3(0.05f, 0.05f, 0.05f);
    material.shininess = 4.0f;
    m_objectMaterials.push_back(material);

    // Desk wood
    material.tag = "woodDesk";
    material.diffuseColor = glm::vec3(0.82f, 0.70f, 0.50f);
    material.specularColor = glm::vec3(0.12f, 0.10f, 0.08f);
    material.shininess = 10.0f;
    m_objectMaterials.push_back(material);

    // Matte black plastic / painted metal
    material.tag = "matteBlack";
    material.diffuseColor = glm::vec3(0.10f, 0.10f, 0.11f);
    material.specularColor = glm::vec3(0.14f, 0.14f, 0.14f);
    material.shininess = 10.0f;
    m_objectMaterials.push_back(material);

    // Metal (monitor stand / lamp arm)
    material.tag = "metal";
    material.diffuseColor = glm::vec3(0.70f, 0.71f, 0.75f);
    material.specularColor = glm::vec3(0.92f, 0.92f, 0.95f);
    material.shininess = 64.0f;
    m_objectMaterials.push_back(material);

    // Bright monitor screen
    material.tag = "screenBlue";
    material.diffuseColor = glm::vec3(0.22f, 0.40f, 0.78f);
    material.specularColor = glm::vec3(0.35f, 0.50f, 0.95f);
    material.shininess = 32.0f;
    m_objectMaterials.push_back(material);

    // Dark screen / tablet
    material.tag = "screenDark";
    material.diffuseColor = glm::vec3(0.10f, 0.14f, 0.20f);
    material.specularColor = glm::vec3(0.22f, 0.28f, 0.35f);
    material.shininess = 28.0f;
    m_objectMaterials.push_back(material);

    // White plastic peripherals
    material.tag = "whitePlastic";
    material.diffuseColor = glm::vec3(0.92f, 0.92f, 0.92f);
    material.specularColor = glm::vec3(0.35f, 0.35f, 0.35f);
    material.shininess = 20.0f;
    m_objectMaterials.push_back(material);

    // Red ceramic cup
    material.tag = "cupRed";
    material.diffuseColor = glm::vec3(0.86f, 0.12f, 0.12f);
    material.specularColor = glm::vec3(0.35f, 0.18f, 0.18f);
    material.shininess = 24.0f;
    m_objectMaterials.push_back(material);

    // Optional compatibility materials
    material.tag = "matteWhite";
    material.diffuseColor = glm::vec3(1.0f, 1.0f, 1.0f);
    material.specularColor = glm::vec3(0.15f, 0.15f, 0.15f);
    material.shininess = 8.0f;
    m_objectMaterials.push_back(material);

    material.tag = "polishedWhite";
    material.diffuseColor = glm::vec3(1.0f, 1.0f, 1.0f);
    material.specularColor = glm::vec3(0.85f, 0.85f, 0.85f);
    material.shininess = 64.0f;
    m_objectMaterials.push_back(material);
}

// =============================================================================
//  LIGHTING SETUP (Phong, multiple point lights, includes tinted light)
// =============================================================================

void SceneManager::SetupSceneLights() {
    if (NULL == m_pShaderManager) return;

    m_pShaderManager->setBoolValue(g_UseLightingName, true);

    // --- Point light 0: Warm key light (main illumination) ---
    m_pShaderManager->setVec3Value("pointLights[0].position",
        glm::vec3(6.0f, 8.0f, 6.0f));
    m_pShaderManager->setFloatValue("pointLights[0].constant", 1.0f);
    m_pShaderManager->setFloatValue("pointLights[0].linear", 0.09f);
    m_pShaderManager->setFloatValue("pointLights[0].quadratic", 0.032f);
    m_pShaderManager->setVec3Value("pointLights[0].ambient",
        glm::vec3(0.16f, 0.16f, 0.16f));
    m_pShaderManager->setVec3Value("pointLights[0].diffuse",
        glm::vec3(0.95f, 0.92f, 0.88f));
    m_pShaderManager->setVec3Value("pointLights[0].specular",
        glm::vec3(1.0f, 0.98f, 0.95f));
    m_pShaderManager->setBoolValue("pointLights[0].bActive", true);

    // --- Point light 1: Blue tinted fill (colored light ? rubric requirement) ---
    m_pShaderManager->setVec3Value("pointLights[1].position",
        glm::vec3(-7.0f, 5.0f, -5.0f));
    m_pShaderManager->setFloatValue("pointLights[1].constant", 1.0f);
    m_pShaderManager->setFloatValue("pointLights[1].linear", 0.14f);
    m_pShaderManager->setFloatValue("pointLights[1].quadratic", 0.07f);
    m_pShaderManager->setVec3Value("pointLights[1].ambient",
        glm::vec3(0.03f, 0.05f, 0.08f));
    m_pShaderManager->setVec3Value("pointLights[1].diffuse",
        glm::vec3(0.20f, 0.35f, 0.70f));
    m_pShaderManager->setVec3Value("pointLights[1].specular",
        glm::vec3(0.30f, 0.45f, 0.80f));
    m_pShaderManager->setBoolValue("pointLights[1].bActive", true);

    // --- Point light 2: Soft rear rim for separation ---
    m_pShaderManager->setVec3Value("pointLights[2].position",
        glm::vec3(0.0f, 7.0f, -8.0f));
    m_pShaderManager->setFloatValue("pointLights[2].constant", 1.0f);
    m_pShaderManager->setFloatValue("pointLights[2].linear", 0.18f);
    m_pShaderManager->setFloatValue("pointLights[2].quadratic", 0.10f);
    m_pShaderManager->setVec3Value("pointLights[2].ambient",
        glm::vec3(0.02f, 0.02f, 0.02f));
    m_pShaderManager->setVec3Value("pointLights[2].diffuse",
        glm::vec3(0.18f, 0.16f, 0.14f));
    m_pShaderManager->setVec3Value("pointLights[2].specular",
        glm::vec3(0.20f, 0.18f, 0.16f));
    m_pShaderManager->setBoolValue("pointLights[2].bActive", true);

    // Disable remaining slots / other light types
    m_pShaderManager->setBoolValue("pointLights[3].bActive", false);
    m_pShaderManager->setBoolValue("directionalLight.bActive", false);
    m_pShaderManager->setBoolValue("spotLight.bActive", false);
}

// =============================================================================
//  PREPARE SCENE (called once at startup)
// =============================================================================

void SceneManager::PrepareScene() {
    // 1. Define materials
    DefineObjectMaterials();

    // 2. Setup lighting
    SetupSceneLights();

    // 3. Load textures
    //    Files must exist in the working directory relative path shown below.
    //    In Visual Studio the working directory is usually the project folder
    //    or the folder containing the .exe ? adjust the paths if needed.
    //
    //    Texture assignments:
    //      "deskTex"  -> desk surface (waffle pattern reads as a light wood grain)
    //      "wallTex"  -> back wall    (ground.png reads as a plaster / concrete)
    //      "metalTex" -> monitor stands / lamp (metallic sheen)
    //
    //    NOTE: If a file fails to load the scene still renders; the object just
    //    uses its flat color instead.  Check the console for load errors.

    CreateGLTexture("textures/waffle.png", "deskTex");
    CreateGLTexture("textures/ground.png", "wallTex");
    CreateGLTexture("textures/metal.png", "metalTex");
    CreateGLTexture("textures/sprinkles.png", "sprinklesTex");

    // Bind all loaded textures to sequential texture units
    BindGLTextures();

    // 4. Load meshes used in the scene
    m_basicMeshes->LoadBoxMesh();
    m_basicMeshes->LoadPlaneMesh();
    m_basicMeshes->LoadCylinderMesh();
    m_basicMeshes->LoadConeMesh();
    m_basicMeshes->LoadSphereMesh();
}

// =============================================================================
//  RENDER SCENE (called every frame)
// =============================================================================

void SceneManager::RenderScene() {
    // Unused variables kept for clarity (match original template style)
    glm::vec3 scaleXYZ;
    float XrotationDegrees = 0.0f;
    float YrotationDegrees = 0.0f;
    float ZrotationDegrees = 0.0f;
    glm::vec3 positionXYZ;

    if (NULL != m_pShaderManager) {
        // Lighting enabled for all objects
        m_pShaderManager->setBoolValue(g_UseLightingName, true);

        // Default: no texture unless explicitly enabled per object
        m_pShaderManager->setBoolValue(g_UseTextureName, false);
    }

    // -------------------------------------------------------------------------
    // Helper lambdas ? reduce repeated transform/color/material/draw calls.
    // These capture 'this' by reference so they can call member functions.
    // -------------------------------------------------------------------------

    auto DrawBox = [&](glm::vec3 s, float rx, float ry, float rz, glm::vec3 p,
        float r, float g, float b, const std::string& matTag) {
            SetTransformations(s, rx, ry, rz, p);
            SetShaderColor(r, g, b, 1.0f);
            SetShaderMaterial(matTag);
            m_basicMeshes->DrawBoxMesh();
        };

    auto DrawPlane = [&](glm::vec3 s, float rx, float ry, float rz, glm::vec3 p,
        float r, float g, float b, const std::string& matTag) {
            SetTransformations(s, rx, ry, rz, p);
            SetShaderColor(r, g, b, 1.0f);
            SetShaderMaterial(matTag);
            m_basicMeshes->DrawPlaneMesh();
        };

    auto DrawCylinder = [&](glm::vec3 s, float rx, float ry, float rz,
        glm::vec3 p, float r, float g, float b,
        const std::string& matTag) {
            SetTransformations(s, rx, ry, rz, p);
            SetShaderColor(r, g, b, 1.0f);
            SetShaderMaterial(matTag);
            m_basicMeshes->DrawCylinderMesh();
        };

    auto DrawCone = [&](glm::vec3 s, float rx, float ry, float rz, glm::vec3 p,
        float r, float g, float b, const std::string& matTag) {
            SetTransformations(s, rx, ry, rz, p);
            SetShaderColor(r, g, b, 1.0f);
            SetShaderMaterial(matTag);
            m_basicMeshes->DrawConeMesh();
        };

    auto DrawSphere = [&](glm::vec3 s, float rx, float ry, float rz, glm::vec3 p,
        float r, float g, float b, const std::string& matTag) {
            SetTransformations(s, rx, ry, rz, p);
            SetShaderColor(r, g, b, 1.0f);
            SetShaderMaterial(matTag);
            m_basicMeshes->DrawSphereMesh();
        };

    // Convenience: enable texture, draw, then disable
    auto EnableTexture = [&](const std::string& texTag, float uScale, float vScale) {
        if (NULL != m_pShaderManager) {
            m_pShaderManager->setBoolValue(g_UseTextureName, true);
            SetShaderTexture(texTag);
            SetTextureUVScale(uScale, vScale);
        }
        };

    auto DisableTexture = [&]() {
        if (NULL != m_pShaderManager) {
            m_pShaderManager->setBoolValue(g_UseTextureName, false);
        }
        };

    // =========================================================================
    // ROOM / DESK ENVIRONMENT
    // =========================================================================

    // --- Desk surface (TEXTURED ? waffle.png gives a subtle pattern) ---
    EnableTexture("deskTex", 4.0f, 2.0f);
    DrawPlane(glm::vec3(18.0f, 1.0f, 8.0f), 0.0f, 0.0f, 0.0f,
        glm::vec3(0.0f, 0.0f, 0.0f), 0.82f, 0.70f, 0.50f, "woodDesk");
    DisableTexture();

    // --- Back wall (TEXTURED ? ground.png gives a plaster/concrete look) ---
    EnableTexture("wallTex", 3.0f, 2.0f);
    DrawPlane(glm::vec3(18.0f, 1.0f, 10.0f), 90.0f, 0.0f, 0.0f,
        glm::vec3(0.0f, 5.0f, -4.0f), 0.90f, 0.89f, 0.84f, "matteWall");
    DisableTexture();

    // Right wall (room corner feel) ? untextured, flat color
    DrawPlane(glm::vec3(8.0f, 1.0f, 10.0f), 90.0f, 90.0f, 0.0f,
        glm::vec3(9.0f, 4.0f, 0.0f), 0.92f, 0.91f, 0.88f, "matteWall");

    // =========================================================================
    // MONITOR RISER SHELF
    // =========================================================================
    DrawBox(glm::vec3(7.8f, 0.35f, 1.8f), 0.0f, 0.0f, 0.0f,
        glm::vec3(0.2f, 0.45f, -1.2f), 0.10f, 0.10f, 0.11f, "matteBlack");

    DrawBox(glm::vec3(0.45f, 0.60f, 1.35f), 0.0f, 0.0f, 0.0f,
        glm::vec3(-3.3f, 0.30f, -1.2f), 0.09f, 0.09f, 0.10f, "matteBlack");

    DrawBox(glm::vec3(0.45f, 0.60f, 1.35f), 0.0f, 0.0f, 0.0f,
        glm::vec3(3.7f, 0.30f, -1.2f), 0.09f, 0.09f, 0.10f, "matteBlack");

    // =========================================================================
    // MAIN MONITOR (CENTER)
    // =========================================================================
    DrawBox(glm::vec3(4.8f, 2.9f, 0.16f), 0.0f, 0.0f, 0.0f,
        glm::vec3(-0.3f, 2.25f, -1.25f), 0.08f, 0.08f, 0.09f, "matteBlack");

    // Screen face ? bright blue glow
    DrawBox(glm::vec3(4.45f, 2.55f, 0.10f), 0.0f, 0.0f, 0.0f,
        glm::vec3(-0.3f, 2.25f, -1.15f), 0.18f, 0.36f, 0.70f, "screenBlue");

    // Monitor stand neck
    DrawBox(glm::vec3(0.25f, 1.00f, 0.20f), 0.0f, 0.0f, 0.0f,
        glm::vec3(-0.3f, 1.10f, -1.20f), 0.70f, 0.71f, 0.75f, "metal");

    // Monitor stand base (TEXTURED ? metal.png for metallic sheen)
    EnableTexture("metalTex", 1.0f, 1.0f);
    DrawBox(glm::vec3(1.35f, 0.08f, 0.85f), 0.0f, 0.0f, 0.0f,
        glm::vec3(-0.3f, 0.55f, -1.10f), 0.70f, 0.71f, 0.75f, "metal");
    DisableTexture();

    // =========================================================================
    // RIGHT VERTICAL MONITOR
    // =========================================================================
    DrawBox(glm::vec3(1.80f, 2.80f, 0.12f), 0.0f, -12.0f, 0.0f,
        glm::vec3(4.2f, 2.15f, -0.95f), 0.08f, 0.08f, 0.09f, "matteBlack");

    DrawBox(glm::vec3(1.55f, 2.50f, 0.08f), 0.0f, -12.0f, 0.0f,
        glm::vec3(4.2f, 2.15f, -0.85f), 0.16f, 0.28f, 0.52f, "screenBlue");

    DrawBox(glm::vec3(0.18f, 0.85f, 0.18f), 0.0f, 0.0f, 0.0f,
        glm::vec3(4.2f, 1.00f, -0.95f), 0.68f, 0.69f, 0.72f, "metal");

    DrawBox(glm::vec3(0.90f, 0.06f, 0.55f), 0.0f, 0.0f, 0.0f,
        glm::vec3(4.2f, 0.55f, -0.95f), 0.68f, 0.69f, 0.72f, "metal");

    // =========================================================================
    // LEFT TABLET / DISPLAY
    // =========================================================================
    DrawBox(glm::vec3(1.60f, 2.20f, 0.10f), -12.0f, 18.0f, 0.0f,
        glm::vec3(-5.0f, 1.35f, 0.6f), 0.07f, 0.07f, 0.08f, "matteBlack");

    DrawBox(glm::vec3(1.40f, 1.95f, 0.06f), -12.0f, 18.0f, 0.0f,
        glm::vec3(-5.0f, 1.35f, 0.68f), 0.12f, 0.16f, 0.22f, "screenDark");

    DrawBox(glm::vec3(0.20f, 0.70f, 0.20f), 0.0f, 0.0f, 0.0f,
        glm::vec3(-5.0f, 0.65f, 0.45f), 0.70f, 0.71f, 0.75f, "metal");

    // =========================================================================
    // KEYBOARD + MOUSE
    // =========================================================================
    DrawBox(glm::vec3(3.10f, 0.12f, 1.15f), 0.0f, 0.0f, 0.0f,
        glm::vec3(-0.2f, 0.12f, 1.40f), 0.92f, 0.92f, 0.92f, "whitePlastic");

    DrawBox(glm::vec3(3.00f, 0.06f, 1.05f), -4.0f, 0.0f, 0.0f,
        glm::vec3(-0.2f, 0.17f, 1.35f), 0.95f, 0.95f, 0.95f, "whitePlastic");

    // Mouse
    DrawSphere(glm::vec3(0.42f, 0.22f, 0.62f), 0.0f, 0.0f, 0.0f,
        glm::vec3(2.35f, 0.18f, 1.35f), 0.90f, 0.90f, 0.90f,
        "whitePlastic");

    // =========================================================================
    // SPEAKERS (LEFT / RIGHT)
    // =========================================================================

    // Left speaker body
    DrawBox(glm::vec3(0.85f, 2.10f, 0.90f), 0.0f, 6.0f, 0.0f,
        glm::vec3(-4.0f, 1.25f, -1.20f), 0.10f, 0.10f, 0.11f, "matteBlack");

    DrawCylinder(glm::vec3(0.25f, 0.25f, 0.12f), 90.0f, 0.0f, 0.0f,
        glm::vec3(-4.0f, 1.75f, -0.72f), 0.18f, 0.18f, 0.20f,
        "matteBlack");

    DrawCylinder(glm::vec3(0.42f, 0.42f, 0.15f), 90.0f, 0.0f, 0.0f,
        glm::vec3(-4.0f, 1.10f, -0.72f), 0.16f, 0.16f, 0.17f,
        "matteBlack");

    // Right speaker body
    DrawBox(glm::vec3(0.85f, 2.10f, 0.90f), 0.0f, -4.0f, 0.0f,
        glm::vec3(2.9f, 1.25f, -1.20f), 0.10f, 0.10f, 0.11f, "matteBlack");

    DrawCylinder(glm::vec3(0.25f, 0.25f, 0.12f), 90.0f, 0.0f, 0.0f,
        glm::vec3(2.9f, 1.75f, -0.72f), 0.18f, 0.18f, 0.20f,
        "matteBlack");

    DrawCylinder(glm::vec3(0.42f, 0.42f, 0.15f), 90.0f, 0.0f, 0.0f,
        glm::vec3(2.9f, 1.10f, -0.72f), 0.16f, 0.16f, 0.17f,
        "matteBlack");

    // =========================================================================
    // DESK ACCESSORIES
    // =========================================================================

    // Red cup (TEXTURED ? sprinkles.png for decorative ceramic)
    EnableTexture("sprinklesTex", 2.0f, 2.0f);
    DrawCylinder(glm::vec3(0.38f, 0.70f, 0.38f), 90.0f, 0.0f, 0.0f,
        glm::vec3(5.7f, 0.35f, 1.10f), 0.86f, 0.12f, 0.12f, "cupRed");
    DisableTexture();

    // Coaster under the cup
    DrawBox(glm::vec3(0.70f, 0.04f, 0.70f), 0.0f, 0.0f, 0.0f,
        glm::vec3(5.7f, 0.03f, 1.10f), 0.18f, 0.14f, 0.10f, "woodDesk");

    // Small pad / device
    DrawBox(glm::vec3(1.10f, 0.12f, 0.70f), 0.0f, -12.0f, 0.0f,
        glm::vec3(4.7f, 0.12f, 0.85f), 0.14f, 0.14f, 0.15f, "matteBlack");

    // =========================================================================
    // LAMP / ARM
    // =========================================================================

    // Lamp base
    DrawCylinder(glm::vec3(0.25f, 0.12f, 0.25f), 90.0f, 0.0f, 0.0f,
        glm::vec3(7.3f, 0.10f, -1.6f), 0.68f, 0.69f, 0.72f, "metal");

    // Lamp arm ? lower segment
    DrawCylinder(glm::vec3(0.10f, 1.70f, 0.10f), 90.0f, 0.0f, 22.0f,
        glm::vec3(7.0f, 1.00f, -1.6f), 0.70f, 0.71f, 0.74f, "metal");

    // Lamp arm ? upper segment
    DrawCylinder(glm::vec3(0.09f, 1.45f, 0.09f), 90.0f, 0.0f, -28.0f,
        glm::vec3(6.4f, 2.15f, -1.35f), 0.70f, 0.71f, 0.74f, "metal");

    // Lamp shade (cone)
    DrawCone(glm::vec3(0.35f, 0.65f, 0.35f), 90.0f, 0.0f, 55.0f,
        glm::vec3(5.9f, 2.70f, -1.05f), 0.12f, 0.12f, 0.13f, "matteBlack");

    // =========================================================================
    // SMALL DETAILS / SCENE POLISH
    // =========================================================================

    // Items under riser shelf
    DrawBox(glm::vec3(0.65f, 0.20f, 0.45f), 0.0f, 0.0f, 0.0f,
        glm::vec3(-1.8f, 0.62f, -0.95f), 0.16f, 0.16f, 0.17f, "matteBlack");

    DrawBox(glm::vec3(0.65f, 0.20f, 0.45f), 0.0f, 0.0f, 0.0f,
        glm::vec3(1.2f, 0.62f, -0.95f), 0.16f, 0.16f, 0.17f, "matteBlack");

    // Hub / dock under riser
    DrawBox(glm::vec3(1.50f, 0.25f, 0.90f), 0.0f, 0.0f, 0.0f,
        glm::vec3(0.8f, 0.16f, -1.10f), 0.13f, 0.13f, 0.14f, "matteBlack");

    // Restore default state (no texture) for any future draws
    DisableTexture();
}
