#include "glad/glad.h"
#include <GLFW/glfw3.h>

#include <stdio.h>
#include <stdlib.h>
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
uniform sampler2D tex;
void main()
{
    gl_FragColor = texture2D(tex, TexCoord);
}
)";

static void errorCallback(int error, const char *description) {
  fprintf(stderr, "Error: %s\n", description);
}

static void keyCallback(GLFWwindow *window, int key, int scancode, int action,
                        int mods) {
  if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
    glfwSetWindowShouldClose(window, GLFW_TRUE);
}

class Window {
private:
  struct Vertex {
    float x, y;
    float u, v;
  };

public:
  Window() {
    GLFWwindow *window;
    GLuint vertex_buffer, tex, vertexShader, fragmentShader, program;
    GLint vPosLocation, vTexLocation;

    glfwSetErrorCallback(errorCallback);

    if (!glfwInit())
      exit(EXIT_FAILURE);

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);

    GLFWmonitor *monitor = glfwGetPrimaryMonitor();
    const GLFWvidmode *mode = glfwGetVideoMode(monitor);

    uint32_t width = mode->width;
    uint32_t height = mode->height;

    window = glfwCreateWindow(width, height, "SSRD", monitor, NULL);

    if (!window) {
      glfwTerminate();
      exit(EXIT_FAILURE);
    }

    glfwSetKeyCallback(window, keyCallback);

    glfwMakeContextCurrent(window);
    gladLoadGL();
    glfwSwapInterval(1);

    Vertex quad[4] = {{-1.0f, -1.0f, 0.0f, 0.0f},
                      {1.0f, -1.0f, 1.0f, 0.0f},
                      {-1.0f, 1.0f, 0.0f, 1.0f},
                      {1.0f, 1.0f, 1.0f, 1.0f}};

    glGenBuffers(1, &vertex_buffer);
    glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);

    vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexShaderT, NULL);
    glCompileShader(vertexShader);

    fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentShaderT, NULL);
    glCompileShader(fragmentShader);

    program = glCreateProgram();
    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);
    glLinkProgram(program);

    vPosLocation = glGetAttribLocation(program, "vPos");
    vTexLocation = glGetAttribLocation(program, "vTexCoord");

    glEnableVertexAttribArray(vPosLocation);
    glVertexAttribPointer(vPosLocation, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          (void *)0);

    glEnableVertexAttribArray(vTexLocation);
    glVertexAttribPointer(vTexLocation, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          (void *)(2 * sizeof(float)));

    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB,
                 GL_UNSIGNED_BYTE, NULL);

    std::vector<uint8_t> frame(width * height * 3);

    while (!glfwWindowShouldClose(window)) {
      glBindTexture(GL_TEXTURE_2D, tex);
      glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RGB,
                      GL_UNSIGNED_BYTE, frame.data());

      glViewport(0, 0, width, height);
      glClear(GL_COLOR_BUFFER_BIT);
      glUseProgram(program);
      glBindTexture(GL_TEXTURE_2D, tex);
      glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

      glfwSwapBuffers(window);
      glfwPollEvents();
    }

    glfwDestroyWindow(window);
    glfwTerminate();
    exit(EXIT_SUCCESS);
  }
};
