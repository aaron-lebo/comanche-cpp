#include <array>
#include <ctime>
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <vector>

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <OpenSimplex/OpenSimplex.h>

using namespace std;
using namespace glm;

GLFWwindow* win;
int width, height;

GLuint vbo, ibo, block_gl;
GLint color_u, mvp_u;

vector<float> heightmap, vertices;
vector<int> indices;

auto color = ImVec4(1, 1, 1, 1);
auto seed = 0,
     size = 100;

auto cursor = true;

auto sensitivity = 0.0005f,
     speed = 30.f,
     fov = 60.f;
auto yaw = radians(45.0), 
     pitch = radians(-15.0);
vec3 direction,
     position(-size, size, -size);

void reseed() {
    seed = rand()%SHRT_MAX+SHRT_MIN;
}

void glfw_err_callback(int err, const char* desc) {
    fprintf(stderr, "glfw error %d: %s\n", err, desc);
}

void key_callback(GLFWwindow* win, int key, int scancode, int action, int mods) {
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
        cursor = !cursor;
        glfwSetInputMode(win, GLFW_CURSOR, cursor ? GLFW_CURSOR_NORMAL : GLFW_CURSOR_DISABLED);
    }
    ImGui_ImplGlfw_KeyCallback(win, key, scancode, action, mods);
}

int glfw_init() {
    glfwSetErrorCallback(glfw_err_callback);
    if (!glfwInit())
        return 1;

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE); // mac 
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    win = glfwCreateWindow(1024, 768, "comanche", nullptr, nullptr);
    if (win == nullptr) 
        return 1;

    glfwMakeContextCurrent(win);
    glfwSetMouseButtonCallback(win, ImGui_ImplGlfw_MouseButtonCallback);
    glfwSetScrollCallback(win, ImGui_ImplGlfw_ScrollCallback);
    glfwSetKeyCallback(win, key_callback);
    glfwSetCharCallback(win, ImGui_ImplGlfw_CharCallback);

    glewExperimental = true; // core profile
    if (glewInit() != GLEW_OK) {
        fprintf(stderr, "glew init failed\n");
        return 1;
    }
}

void check_status(GLuint id, GLenum status) {
    auto get_iv = glGetShaderiv;
    auto get_log = glGetShaderInfoLog;
    if (status == GL_LINK_STATUS) {
        get_iv = glGetProgramiv;
        get_log = glGetProgramInfoLog;
    }
    GLint ok;
    get_iv(id, status, &ok);
    if (ok == GL_FALSE) {
        int len;
        get_iv(id, GL_INFO_LOG_LENGTH, &len);
        vector<char> err(len+1);
        get_log(id, len, nullptr, &err[0]);
        fprintf(stderr, "%s\n", &err[0]);
    }
}

GLuint create_shader(GLenum shader, const char* src) {
    auto id = glCreateShader(shader);
    glShaderSource(id, 1, &src, nullptr);
    glCompileShader(id);
    check_status(id, GL_COMPILE_STATUS);
    return id;
}

GLuint load_glsl(const char* vertex_src, const char* fragment_src) {
    auto id = glCreateProgram(),
         vert = create_shader(GL_VERTEX_SHADER, vertex_src),
         frag = create_shader(GL_FRAGMENT_SHADER, fragment_src);
    glAttachShader(id, vert);
    glAttachShader(id, frag);
    glLinkProgram(id);
    check_status(id, GL_LINK_STATUS);
    glDetachShader(id, vert);
    glDetachShader(id, frag);
    glDeleteShader(vert);
    glDeleteShader(frag);
    return id;
}

void add_face(initializer_list<int> face, bool cond) {
    if (cond)
        for (int i : face) indices.push_back(i+(vertices.size()-36)/3);
}

void add_block(int x, int z) {
    const auto a = 0.4f;
    auto y = heightmap[x*size+z],
         y0 = z == size-1 ? -1 : a-y+heightmap[x*size+z+1],
         y1 = z == 0 ? -1 : a-y+heightmap[x*size+z-1],
         y2 = x == size-1 ? -1 : a-y+heightmap[(x+1)*size+z],
         y3 = x == 0 ? -1 : a-y+heightmap[(x-1)*size+z];
    array<float, 36> verts {
        -a, y0,  a,
         a, y0,  a,
         a,  a,  a,
        -a,  a,  a,
        -a, y1, -a,
        -a,  a, -a,
         a,  a, -a,
         a, y1, -a,
        -a, y3,  a, 
         a, y2,  a,
        -a, y3, -a,
         a, y2, -a
    };
    for (int i = 0; i < verts.size();)
        vertices.insert(vertices.end(), {verts[i++]+x, verts[i++]+y, verts[i++]+z});

    add_face({0,  1, 2, 2,  3, 0}, y0 > a && z < size-1); // +z
    add_face({4,  5, 6, 6,  7, 4}, y1 > a && z > 0); // -z
    add_face({3,  2, 6, 6,  5, 3}, true); // +y
    add_face({9, 11, 6, 6,  2, 9}, y2 > a && x < size-1); // +x
    add_face({8,  3, 5, 5, 10, 8}, y3 > a && x > 0); // -x
}

void gen_map() {
    OpenSimplex::Context ctx;
    OpenSimplex::Seed::computeContextForSeed(ctx, seed);

    heightmap.clear();
    heightmap.resize(size*size);
    for (int x = 0; x < size; x++)
        for (int z = 0; z < size; z++)
            heightmap[x*size+z] = size*(OpenSimplex::Noise::noise2(ctx, (double)x/size-0.5, (double)z/size-0.5)/2+0.5);

    vertices.clear();
    indices.clear();
    for (int x = 0; x < size; x++)
        for (int z = 0; z < size; z++)
            add_block(x, z);

    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, vertices.size()*sizeof(float), &vertices[0], GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size()*sizeof(int), &indices[0], GL_STATIC_DRAW);
}

mat4 get_matrix() {
    static auto last_time = glfwGetTime();
    auto now = glfwGetTime();

    if (!cursor) {
        double x, y;
        glfwGetCursorPos(win, &x, &y);
        glfwSetCursorPos(win, width/2, height/2);

        yaw += sensitivity*float(width/2-x);
        yaw = abs(degrees(yaw)) > 360 ? 0 : yaw;

        const auto max = radians(89.9);
        pitch += sensitivity*float(height/2-y);
        pitch = pitch < -max ? -max :
            pitch > max ? max :
            pitch;
    }

    vec3 right, up;
    right = vec3(
        sin(yaw-3.14f/2),
        0,
        cos(yaw-3.14f/2)
    );
    direction = vec3(
        cos(pitch)*sin(yaw),
        sin(pitch),
        cos(pitch)*cos(yaw)
    );
    up = cross(right, direction);
    if (!cursor) {
        auto s = speed*float(now - last_time);
        if (glfwGetKey(win, GLFW_KEY_W) == GLFW_PRESS) position += s*direction;
        if (glfwGetKey(win, GLFW_KEY_A) == GLFW_PRESS) position -= s*right;
        if (glfwGetKey(win, GLFW_KEY_S) == GLFW_PRESS) position -= s*direction;
        if (glfwGetKey(win, GLFW_KEY_D) == GLFW_PRESS) position += s*right;
    }

    auto mvp = perspective(radians(fov), float(width/height), 0.01f, 1000.f) *
        lookAt(position, position+direction, up) *
        mat4(1);
    last_time = now;
    return mvp;
}

void render() {
    glUseProgram(block_gl);
    glUniform4f(color_u, color.x, color.y, color.z, color.w);
    glUniformMatrix4fv(mvp_u, 1, false, &get_matrix()[0][0]);

    glEnableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glVertexAttribPointer(0, 3, GL_FLOAT, false, 0, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
    glDrawElements(GL_TRIANGLES, indices.size(), GL_UNSIGNED_INT, nullptr);
    glDisableVertexAttribArray(0);
}

void render_ui() {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    ImGui::SetNextWindowSize(ImVec2(400, 450), ImGuiCond_FirstUseEver);
    ImGui::Begin("comanche");

    ImGui::ColorEdit3("color", (float*)&color);
    ImGui::InputInt("seed", &seed);
    ImGui::InputInt("size", &size);
    if (ImGui::Button("reseed")) reseed();
    ImGui::SameLine(); 
    if (ImGui::Button("generate map")) gen_map();
    ImGui::Separator();

    ImGui::SliderFloat("sensitivity", &sensitivity, 0.0001, 0.001, nullptr);
    ImGui::SliderFloat("speed (m/s)", &speed, 0.1, 1000, "%.1f");
    ImGui::SliderFloat("fov", &fov, 45, 120, "%.1f");
    ImGui::Separator();

    auto io = ImGui::GetIO();
    ImGui::Text("fps: %.2f", io.Framerate);
    ImGui::Text("yaw: %.2f", degrees(yaw));
    ImGui::Text("pitch: %.2f", degrees(pitch));
    ImGui::Text("position: %.2f, %.2f, %.2f", position.x, position.y, position.z);
    ImGui::Text("direction: %.2f, %.2f, %.2f", direction.x, direction.y, direction.z);
    ImGui::Separator();

    ImGui::Text("ESC - toggle input");
    ImGui::Text("w - forward");
    ImGui::Text("a - left");
    ImGui::Text("s - backward");
    ImGui::Text("d - right");
    ImGui::Separator();

    if (ImGui::Button("quit")) glfwSetWindowShouldClose(win, true);

    ImGui::End();
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

int main() {
    srand(time(0));
    reseed();

    glfw_init();
    if (win == nullptr) 
        return 1;

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui_ImplGlfw_InitForOpenGL(win, false);
    ImGui_ImplOpenGL3_Init("#version 150");
    ImGui::StyleColorsDark();

    glClearColor(0, 0, 0, 0);
    glEnable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);

    GLuint vao;
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    glGenBuffers(1, &vbo);
    glGenBuffers(1, &ibo);

    block_gl = load_glsl(
        R"(
        #version 330 core
        layout(location = 0) in vec3 xyz;
        uniform mat4 mvp;
        void main() {	
            gl_Position = mvp * vec4(xyz, 1);
        })",
        R"(
        #version 330 core
        uniform vec4 color;
        void main() {
            gl_FragColor = color;
        })"
    );
    color_u = glGetUniformLocation(block_gl, "color");
    mvp_u = glGetUniformLocation(block_gl, "mvp");

    gen_map();
    while (!glfwWindowShouldClose(win)) {
        glfwGetFramebufferSize(win, &width, &height);
        glViewport(0, 0, width, height);
        glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);

        render();
        render_ui();

        if (seed < SHRT_MIN) seed = SHRT_MIN;
        if (seed > SHRT_MAX) seed = SHRT_MAX;
        if (size < 2) size = 2;
        if (size > 2500) size = 2500;

        glfwSwapBuffers(win);
        glfwPollEvents();
    }

    glDeleteProgram(block_gl);
    glDeleteBuffers(1, &vbo);
    glDeleteBuffers(1, &ibo);
    glDeleteVertexArrays(1, &vao);

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(win);    
    glfwTerminate();
}
