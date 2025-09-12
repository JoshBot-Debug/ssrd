#pragma once

#include "glad/glad.h"
#include <GLFW/glfw3.h>

#include <functional>
#include <stdio.h>
#include <stdlib.h>
#include <thread>
#include <vector>

static const char *vertexShaderT = R"(
#version 110
attribute vec2 vPos;
attribute vec2 vTexCoord;
varying vec2 TexCoord;
void main()
{
    gl_Position = vec4(vPos, 0.0, 1.0);
    TexCoord = vTexCoord;
}
)";

static const char *fragmentShaderT = R"(
#version 110
varying vec2 TexCoord;
uniform sampler2D m_Texture;
void main()
{
    vec2 flippedUV = vec2(TexCoord.x, 1.0 - TexCoord.y);
    gl_FragColor = texture2D(m_Texture, flippedUV);
}
)";

static void errorCallback(int error, const char *description) {
  fprintf(stderr, "Error: %s\n", description);
}

class Window {
private:
  struct Vertex {
    float x, y;
    float u, v;
  };

  struct Init {
    void *data = nullptr;
    GLFWframebuffersizefun onResize = nullptr;
    GLFWkeyfun onKeyPress = nullptr;
    GLFWcursorposfun onMouseMove = nullptr;
    GLFWmousebuttonfun onMouseButton = nullptr;
    GLFWscrollfun onScroll = nullptr;
  };

private:
  GLFWwindow *m_Window = nullptr;

  GLuint m_VertexBuffer = 0, m_Texture = 0, m_VertexShader = 0,
         m_FragmentShader = 0, m_Program = 0;
  GLint m_VPosLocation = 0, m_VTexLocation = 0;

  uint32_t m_TexWidth = 0, m_TexHeight = 0;

  std::vector<uint8_t> m_Buffer = {};

  Init m_Init;

public:
  Window() = default;

  ~Window() {
    if (m_Program)
      glDeleteProgram(m_Program);

    if (m_Window)
      glfwDestroyWindow(m_Window);

    glfwTerminate();
  }

  void initialize(Init init) {
    m_Init = init;

    glfwSetErrorCallback(errorCallback);

    if (!glfwInit())
      exit(EXIT_FAILURE);

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);

    GLFWmonitor *monitor = glfwGetPrimaryMonitor();
    const GLFWvidmode *mode = glfwGetVideoMode(monitor);

    m_Window = glfwCreateWindow(mode->width, mode->height, "ssrd", monitor,
    nullptr);
    // m_Window = glfwCreateWindow(800, 600, "ssrd", nullptr, nullptr);

    glfwSetWindowUserPointer(m_Window, init.data);
    glfwSetKeyCallback(m_Window, init.onKeyPress);
    glfwSetCursorPosCallback(m_Window, init.onMouseMove);
    glfwSetFramebufferSizeCallback(m_Window, init.onResize);
    glfwSetMouseButtonCallback(m_Window, init.onMouseButton);
    glfwSetScrollCallback(m_Window, init.onScroll);

    if (!m_Window) {
      glfwTerminate();
      exit(EXIT_FAILURE);
    }

    glfwMakeContextCurrent(m_Window);
    gladLoadGL();
    glfwSwapInterval(1);

    Vertex quad[4] = {{-1.0f, -1.0f, 0.0f, 0.0f},
                      {1.0f, -1.0f, 1.0f, 0.0f},
                      {-1.0f, 1.0f, 0.0f, 1.0f},
                      {1.0f, 1.0f, 1.0f, 1.0f}};

    glGenBuffers(1, &m_VertexBuffer);
    glBindBuffer(GL_ARRAY_BUFFER, m_VertexBuffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);

    m_VertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(m_VertexShader, 1, &vertexShaderT, NULL);
    glCompileShader(m_VertexShader);

    m_FragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(m_FragmentShader, 1, &fragmentShaderT, NULL);
    glCompileShader(m_FragmentShader);

    m_Program = glCreateProgram();
    glAttachShader(m_Program, m_VertexShader);
    glAttachShader(m_Program, m_FragmentShader);
    glLinkProgram(m_Program);

    glDeleteShader(m_VertexShader);
    glDeleteShader(m_FragmentShader);

    m_VPosLocation = glGetAttribLocation(m_Program, "vPos");
    m_VTexLocation = glGetAttribLocation(m_Program, "vTexCoord");

    glEnableVertexAttribArray(m_VPosLocation);
    glVertexAttribPointer(m_VPosLocation, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          (void *)0);

    glEnableVertexAttribArray(m_VTexLocation);
    glVertexAttribPointer(m_VTexLocation, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          (void *)(2 * sizeof(float)));
  }

  bool resize(uint32_t width, uint32_t height) {
    if (m_TexWidth == width && m_TexHeight == height)
      return false;

    m_TexWidth = width;
    m_TexHeight = height;

    if (m_Texture)
      glDeleteTextures(1, &m_Texture);

    glGenTextures(1, &m_Texture);
    glBindTexture(GL_TEXTURE_2D, m_Texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB,
                 GL_UNSIGNED_BYTE, NULL);

    //  Trigger a resize, aspect ratio may have changed
    {
      int w, h;
      glfwGetFramebufferSize(m_Window, &w, &h);
      m_Init.onResize(m_Window, w, h);
    }

    return true;
  }

  void present(uint32_t width, uint32_t height) {
    resize(width, height);

    glBindTexture(GL_TEXTURE_2D, m_Texture);

    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RGB,
                    GL_UNSIGNED_BYTE, m_Buffer.data());

    glClear(GL_COLOR_BUFFER_BIT);
    glUseProgram(m_Program);
    glBindTexture(GL_TEXTURE_2D, m_Texture);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    glfwSwapBuffers(m_Window);
    glfwPollEvents();
  }

  int shouldClose() { return glfwWindowShouldClose(m_Window); }

  void setBuffer(const std::vector<uint8_t> &buffer) { m_Buffer = buffer; }
};
