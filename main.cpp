#include <iostream>
#include <GL/glew.h>
#include <OpenGL/gl.h>
#include <GLFW/glfw3.h>

#include "framer.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include "main.h"

GLuint image2Texture(const std::string& file, int& width, int& height, int& numChannels);
GLuint image2Texture(const std::string& file);

const int window_width = 800;
const int window_height = 600;

const std::string vertex_shader = R"(
#version 330 core
layout (location = 0) in vec2 position;
layout (location = 1) in vec2 texCoord;

out vec2 fragTexCoord;

void main()
{
    gl_Position = vec4(position, 0.0, 1.0);
    fragTexCoord = texCoord;
}
)";

const std::string frag_shader = R"(
#version 330 core
in vec2 fragTexCoord;
out vec4 FragColor;

uniform sampler2D tex;

void main()
{
    FragColor = texture(tex, fragTexCoord);
}   
)";

GLuint compileShader(GLenum shaderType, const std::string& shaderCode) {
    GLuint shader = glCreateShader(shaderType);
    const char* code = shaderCode.c_str();
    glShaderSource(shader, 1, &code, nullptr);
    glCompileShader(shader);

    //check for compilation error
    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        GLchar info[512];
        glGetShaderInfoLog(shader, 512, nullptr, info);
        std::cerr << "shader compilation error: " << info << std::endl;
        return -1;
    }
    return shader;
}

GLuint createVAO(GLuint pos, float* data, int cnt, int elementPerVertex) {
    GLuint vao, vbo;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);

    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, cnt * sizeof(float), data, GL_STATIC_DRAW);
    glVertexAttribPointer(pos, 2, GL_FLOAT, GL_FALSE, elementPerVertex * sizeof(float), 0);
    glEnableVertexAttribArray(pos);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    return vao;
}

struct Vertex {
    float pos[2];
    float tex[2];
};

int main() {
    if (!glfwInit()) {
        std::cerr<<"ERROR: could not start GLFW3"<<std::endl;
        return -1;
    }

    //Setting window properties
    glfwWindowHint (GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint (GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint (GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    glfwWindowHint (GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(window_width, window_height, "OpenGL Demo", NULL, NULL);
    if (!window) {
        std::cerr<<"ERROR: could not open window with GLFW3"<<std::endl;
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);

    if (glewInit() != GLEW_OK) {
        glfwTerminate();
        return -1;
    }

     // get version info
    const GLubyte* renderer = glGetString (GL_RENDERER); // get renderer string
    const GLubyte* version = glGetString (GL_VERSION); // version as a string
    std::cout<<"Renderer: "<<renderer<<std::endl;
    std::cout<<"OpenGL version supported "<<version<<std::endl;

    //pipeline
    //setup program
    GLuint vertexShader = compileShader(GL_VERTEX_SHADER, vertex_shader);
    GLuint fragShader = compileShader(GL_FRAGMENT_SHADER, frag_shader);
    GLuint program = glCreateProgram();
    glAttachShader(program, vertexShader);
    glAttachShader(program, fragShader);
    glLinkProgram(program);
    GLint success;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        GLchar info[512];
        glGetProgramInfoLog(program, 512, nullptr, info);
        std::cerr << "shader program linking error: " << info << std::endl;
        glfwTerminate();
        return -1;
    }
    glDeleteShader(vertexShader);
    glDeleteShader(fragShader);

    glUseProgram(program);
    assert(GL_NO_ERROR == glGetError());

    //load image into texture
    // GLuint texture = image2Texture("tifa-540x960.jpg");
    GLuint texture = image2Texture("ada-1920x1080.jpg");
    if (texture == -1) {
        glfwTerminate();
        return -1;
    }
    //setup vertex data
    using namespace WDFramer;
    Framer framer(
        // {540, 960},
        {1920, 1080},
        {window_width, window_height}, 
        Framer::origin_t::bottom_left, 
        Framer::mode_t::aspectFill
    );
    float vertices[8], texcoord[8];
    framer.gl_fullscreen_quad(vertices, texcoord, Framer::rot_t::cw_180, true);

    Vertex verts[4];
    for (int i = 0; i < 4; i++) {
        verts[i].pos[0] = vertices[i*2 + 0];
        verts[i].pos[1] = vertices[i*2 + 1];
        verts[i].tex[0] = texcoord[i*2 + 0];
        verts[i].tex[1] = texcoord[i*2 + 1];
    }

    GLuint vao, vbo;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);

    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, 4 * sizeof(Vertex), verts, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    assert(GL_NO_ERROR == glGetError());
    GLenum err = glGetError();

    GLuint textureSamplerLoc = glGetUniformLocation(program, "tex");
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);
    glUniform1i(textureSamplerLoc, 0);

    assert(GL_NO_ERROR == glGetError());

    while (!glfwWindowShouldClose(window)) {
        glClearColor(0.2f, 0.4f, 0.8f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        glBindVertexArray(vao);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

        assert(GL_NO_ERROR == glGetError());

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glDeleteProgram(program);

    glfwTerminate();
    return 0;
}


GLuint image2Texture(const std::string& file) {
    int w, h, c;
    return image2Texture(file, w, h, c);
}

GLuint image2Texture(const std::string& file, int& width, int& height, int& numChannels) {
    // load a image into texture
    unsigned char *imageData = stbi_load(file.c_str(), &width, &height, &numChannels, 0);
    if (!imageData)
    {
        std::cerr << "failed to load image" << std::endl;
        return -1;
    }

    GLuint textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);

    // Set texture parameters (e.g., filtering, wrapping)
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // Specify the image data for the texture
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, imageData);
    // glGenerateMipmap(GL_TEXTURE_2D);

    // Free the image data
    stbi_image_free(imageData);
    return textureID;
}