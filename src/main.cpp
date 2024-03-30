#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <unistd.h>
#include <stdio.h>
#include <cyTriMesh.h>
#include <string.h>
#include <fstream>
#include <sstream>
// #include <cyGL.h>
#include <cyMatrix.h>
#include <lodepng.h>
#include <vector>
#include <map>
#include <math.h>

struct Particle
{
public:
    cy::Vec4f pos;
    cy::Vec4f vel;
    double mass;
    double padding;
};

// window variables
GLFWwindow *WINDOW;
int WIDTH = 900;
int HEIGHT = 600;
GLuint vao;
bool paused = true;

// particle variables
int PARTICLE_COUNT = 2500;
float MINIMUM_MASS = 1.0;
float MAXIMUM_MASS = 50.0;
float VELOCITY_CUTOFF = 0.3; // fastest color beyond this point
std::vector<Particle> particles;
std::vector<int> particleIndices;
GLuint particleInputBuffer;
GLuint particleOutputBuffer;
GLuint particleIndexBuffer;

// camera movement variables
float sensitivity = 0.005;
bool leftMouseDown;
bool rightMouseDown;
float prevX = -10000;
float prevY = -10000;
float rotationY;
float rotationX;
float cameraDistance;
cy::Vec3f cameraPos;
cy::Matrix3f rotation;
cy::Matrix3f rotationInverse;
cy::Matrix4f projMatrix;
cy::Matrix4f viewMatrix;
cy::Matrix4f viewMatrixInverse;

// particle shader variables
GLuint particleProgramID;
GLuint particleMatrixID;
GLuint particleRotationID;
GLuint velocityCutoffID;

// cubemap shader variables
GLuint quadProgramID;
GLuint CubemapTexture;
GLuint EnvBuffer;
GLuint EnvTextureID;
GLuint EnvMatrixID;

// gravity shader variables
GLuint gravityProgramID;

void updateViewMatrix();

// helper random number generator
float randFloat(float min, float max)
{
    float r = ((float)rand()) / (float)RAND_MAX;
    float diff = max - min;
    return min + r * diff;
}

// Prints any errors that may occur with OpenGL
void glfwErrorCallback(int error, const char *description)
{
    std::cerr << "GLFW Error " << error << ": " << description << std::endl;
    exit(1);
}

// Key callback
// Esc closes the program
// F6 reloads shaders
void glfwKeyCallback(GLFWwindow *p_window, int p_key, int p_scancode, int p_action, int p_mods)
{
    if (p_key == GLFW_KEY_ESCAPE && p_action == GLFW_PRESS)
    {
        glfwSetWindowShouldClose(WINDOW, GL_TRUE);
    }
    else if (p_key == GLFW_KEY_SPACE && p_action == GLFW_RELEASE)
    {
        paused = !paused;
    }
    else if (p_key == GLFW_KEY_R && p_action == GLFW_RELEASE)
    {
        glBindBuffer(GL_ARRAY_BUFFER, particleIndexBuffer);
        glBufferData(GL_ARRAY_BUFFER, sizeof(int) * particleIndices.size(), &particleIndices[0], GL_STATIC_DRAW);
        glBindBuffer(GL_ARRAY_BUFFER, 0);

        glBindBuffer(GL_SHADER_STORAGE_BUFFER, particleInputBuffer);
        glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(Particle) * particles.size(), &particles[0], GL_DYNAMIC_COPY);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

        glBindBuffer(GL_SHADER_STORAGE_BUFFER, particleOutputBuffer);
        glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(Particle) * particles.size(), &particles[0], GL_DYNAMIC_DRAW);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    }
}

// Mouse move callback
// If left click dragging, rotates the object
// If right click dragging, zooms in on the object.
void glfwMouseMoveCallback(GLFWwindow *window, double xpos, double ypos)
{
    if (leftMouseDown || rightMouseDown)
    {
        if (prevX < -9999)
        {
            prevX = xpos;
            prevY = ypos;
        }
        if (leftMouseDown)
        {
            rotationY += sensitivity * (xpos - prevX);
            rotationX += sensitivity * (ypos - prevY);
            rotation = cy::Matrix3f::RotationX(rotationX) * cy::Matrix3f::RotationY(rotationY);
            rotationInverse = rotation;
            rotationInverse.Invert();
        }
        if (rightMouseDown)
        {
            cameraDistance += (ypos - prevY);
        }
        prevX = xpos;
        prevY = ypos;
        updateViewMatrix();
    }
}

// Loads and reloads shaders
void loadShaders(const char *vertexShader, const char *fragmentShader, const char *geometryShader, GLuint &programID, std::map<const char *, GLuint *> shaderArgs)
{
    GLuint vertShaderID = glCreateShader(GL_VERTEX_SHADER);
    std::ifstream vertShaderStream(vertexShader, std::ios::in);
    std::stringstream vsstr;
    vsstr << vertShaderStream.rdbuf();
    std::string vertShaderCode = vsstr.str();
    vertShaderStream.close();

    GLuint fragShaderID = glCreateShader(GL_FRAGMENT_SHADER);
    std::ifstream fragShaderStream(fragmentShader, std::ios::in);
    std::stringstream fsstr;
    fsstr << fragShaderStream.rdbuf();
    std::string fragShaderCode = fsstr.str();
    fragShaderStream.close();

    GLuint geomShaderID;
    std::string geomShaderCode;
    if (geometryShader != nullptr)
    {
        geomShaderID = glCreateShader(GL_GEOMETRY_SHADER);
        std::ifstream geomShaderStream(geometryShader, std::ios::in);
        std::stringstream gsstr;
        gsstr << geomShaderStream.rdbuf();
        geomShaderCode = gsstr.str();
        geomShaderStream.close();
    }

    GLint result = GL_FALSE;
    int infoLogLength;

    const char *vertSourcePointer = vertShaderCode.c_str();
    glShaderSource(vertShaderID, 1, &vertSourcePointer, NULL);
    glCompileShader(vertShaderID);

    glGetShaderiv(vertShaderID, GL_COMPILE_STATUS, &result);
    glGetShaderiv(vertShaderID, GL_INFO_LOG_LENGTH, &infoLogLength);
    if (infoLogLength > 0)
    {
        char VertexShaderErrorMessage[infoLogLength + 1];
        glGetShaderInfoLog(vertShaderID, infoLogLength, NULL, &VertexShaderErrorMessage[0]);
        std::cout << VertexShaderErrorMessage << std::endl;
    }

    const char *fragSourcePointer = fragShaderCode.c_str();
    glShaderSource(fragShaderID, 1, &fragSourcePointer, NULL);
    glCompileShader(fragShaderID);

    glGetShaderiv(fragShaderID, GL_COMPILE_STATUS, &result);
    glGetShaderiv(fragShaderID, GL_INFO_LOG_LENGTH, &infoLogLength);
    if (infoLogLength > 0)
    {
        char FragmentShaderErrorMessage[infoLogLength + 1];
        glGetShaderInfoLog(fragShaderID, infoLogLength, NULL, &FragmentShaderErrorMessage[0]);
        std::cout << FragmentShaderErrorMessage << std::endl;
    }

    if (geometryShader != nullptr)
    {
        const char *geomSourcePointer = geomShaderCode.c_str();
        glShaderSource(geomShaderID, 1, &geomSourcePointer, NULL);
        glCompileShader(geomShaderID);

        glGetShaderiv(geomShaderID, GL_COMPILE_STATUS, &result);
        glGetShaderiv(geomShaderID, GL_INFO_LOG_LENGTH, &infoLogLength);
        if (infoLogLength > 0)
        {
            char GeometryShaderErrorMessage[infoLogLength + 1];
            glGetShaderInfoLog(geomShaderID, infoLogLength, NULL, &GeometryShaderErrorMessage[0]);
            std::cout << GeometryShaderErrorMessage << std::endl;
        }
    }

    glDeleteProgram(programID);

    programID = glCreateProgram();
    glAttachShader(programID, vertShaderID);
    glAttachShader(programID, fragShaderID);
    if (geometryShader != nullptr)
    {
        glAttachShader(programID, geomShaderID);
    }
    glLinkProgram(programID);

    glDetachShader(programID, vertShaderID);
    glDetachShader(programID, fragShaderID);
    if (geometryShader != nullptr)
    {
        glDetachShader(programID, geomShaderID);
    }

    glDeleteShader(vertShaderID);
    glDeleteShader(fragShaderID);
    if (geometryShader != nullptr)
    {
        glDeleteShader(geomShaderID);
    }

    glUseProgram(programID);

    // load the keys into a vector so we can itterate the map and modify it
    std::vector<const char *> keys;
    for (std::map<const char *, GLuint *>::iterator it = shaderArgs.begin(); it != shaderArgs.end(); it++)
    {
        keys.push_back(it->first);
    }

    for (int i = 0; i < keys.size(); i++)
    {
        const char *key = keys[i];
        *(shaderArgs[key]) = glGetUniformLocation(programID, key);
    }
}

void loadShaders(const char *vertexShader, const char *fragmentShader, GLuint &programID, std::map<const char *, GLuint *> shaderArgs)
{
    loadShaders(vertexShader, fragmentShader, nullptr, programID, shaderArgs);
}

void loadComputeShader(const char *computeShader, GLuint &programID, std::map<const char *, GLuint *> shaderArgs)
{
    GLuint compShaderID = glCreateShader(GL_COMPUTE_SHADER);
    std::ifstream compShaderStream(computeShader, std::ios::in);
    std::stringstream csstr;
    csstr << compShaderStream.rdbuf();
    std::string compShaderCode = csstr.str();
    compShaderStream.close();

    GLint result = GL_FALSE;
    int infoLogLength;

    const char *compSourcePointer = compShaderCode.c_str();
    glShaderSource(compShaderID, 1, &compSourcePointer, NULL);
    glCompileShader(compShaderID);

    glGetShaderiv(compShaderID, GL_COMPILE_STATUS, &result);
    glGetShaderiv(compShaderID, GL_INFO_LOG_LENGTH, &infoLogLength);
    if (infoLogLength > 0)
    {
        char ComputeShaderErrorMessage[infoLogLength + 1];
        glGetShaderInfoLog(compShaderID, infoLogLength, NULL, &ComputeShaderErrorMessage[0]);
        std::cout << ComputeShaderErrorMessage << std::endl;
    }

    glDeleteProgram(programID);

    programID = glCreateProgram();
    glAttachShader(programID, compShaderID);
    glLinkProgram(programID);

    glDetachShader(programID, compShaderID);

    glDeleteShader(compShaderID);

    glUseProgram(programID);

    // load the keys into a vector so we can itterate the map and modify it
    std::vector<const char *> keys;
    for (std::map<const char *, GLuint *>::iterator it = shaderArgs.begin(); it != shaderArgs.end(); it++)
    {
        keys.push_back(it->first);
    }

    for (int i = 0; i < keys.size(); i++)
    {
        const char *key = keys[i];
        *(shaderArgs[key]) = glGetUniformLocation(programID, key);
    }
}

void loadCubeMap(GLuint &texture)
{
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_CUBE_MAP, texture);

    unsigned width, height, error;

    std::vector<unsigned char> posXTexBuff;
    error = lodepng::decode(posXTexBuff, width, height, "cubemaps\\cubemap_posx.png");
    glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, &posXTexBuff[0]);

    std::vector<unsigned char> negXTexBuff;
    error = lodepng::decode(negXTexBuff, width, height, "cubemaps\\cubemap_negx.png");
    glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_X, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, &negXTexBuff[0]);

    std::vector<unsigned char> posYTexBuff;
    error = lodepng::decode(posYTexBuff, width, height, "cubemaps\\cubemap_posy.png");
    glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Y, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, &posYTexBuff[0]);

    std::vector<unsigned char> negYTexBuff;
    error = lodepng::decode(negYTexBuff, width, height, "cubemaps\\cubemap_negy.png");
    glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, &negYTexBuff[0]);

    std::vector<unsigned char> posZTexBuff;
    error = lodepng::decode(posZTexBuff, width, height, "cubemaps\\cubemap_posz.png");
    glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Z, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, &posZTexBuff[0]);

    std::vector<unsigned char> negZTexBuff;
    error = lodepng::decode(negZTexBuff, width, height, "cubemaps\\cubemap_negz.png");
    glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, &negZTexBuff[0]);

    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
}

// Updates the mvp after parameters have changed, and updates the uniform opengl reference
void updateViewMatrix()
{
    cy::Matrix4f translation = cy::Matrix4f::Translation(cy::Vec3f(0.0, 0.0, -cameraDistance));
    viewMatrix = projMatrix * translation * rotation;
    viewMatrixInverse = projMatrix * rotation;
    viewMatrixInverse.Invert();

    cameraPos = (cy::Vec3f(-cameraDistance * sin(rotationY) * cos(rotationX), cameraDistance * sin(rotationX), cameraDistance * cos(rotationY) * cos(rotationX)));
}

// Default values needed to compute the mvp
void initViewMatrix()
{
    rotation = cy::Matrix3f::Identity();
    cameraDistance = 30;
    rotationY = -0.5;
    rotationX = 0.5;

    rotation = cy::Matrix3f::RotationX(rotationX) * cy::Matrix3f::RotationY(rotationY);
    rotationInverse = rotation;
    rotationInverse.Invert();
    projMatrix = cy::Matrix4f::Perspective(0.698132, float(WIDTH) / float(HEIGHT), 0.1f, 1000.0f);

    updateViewMatrix();
}

// Mouse button callback
// Currently keeps track of if the left and right mouse button are down
void glfwMouseButtonCallback(GLFWwindow *window, int button, int action, int mods)
{
    if (button == GLFW_MOUSE_BUTTON_LEFT)
    {
        if (GLFW_PRESS == action)
            leftMouseDown = true;
        else if (GLFW_RELEASE == action)
        {
            leftMouseDown = false;
            prevX = -10000;
            prevY = -10000;
        }
    }

    if (button == GLFW_MOUSE_BUTTON_RIGHT)
    {
        if (GLFW_PRESS == action)
            rightMouseDown = true;
        else if (GLFW_RELEASE == action)
        {
            rightMouseDown = false;
            prevX = -10000;
            prevY = -10000;
        }
    }
}

// Initalizes GLFW and GLEW
void initWindow()
{
    // initalize GLFW
    glfwSetErrorCallback(glfwErrorCallback);
    if (!glfwInit())
    {
        std::cerr << "GLFW Error: Could not initalize GLFW library!" << std::endl;
        exit(1);
    }

    glfwWindowHint(GLFW_SAMPLES, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    WINDOW = glfwCreateWindow(WIDTH, HEIGHT, "3D N-Body", NULL, NULL);
    if (!WINDOW)
    {
        glfwTerminate();
        std::cerr << "GLFW Error: Could not initalize window!" << std::endl;
        exit(1);
    }

    // callbacks
    glfwSetKeyCallback(WINDOW, glfwKeyCallback);
    glfwSetCursorPosCallback(WINDOW, glfwMouseMoveCallback);
    glfwSetMouseButtonCallback(WINDOW, glfwMouseButtonCallback);

    glfwMakeContextCurrent(WINDOW);

    // turn on VSYNC
    glfwSwapInterval(1);

    glewExperimental = GL_TRUE;
    GLenum err = glewInit();
    if (GLEW_OK != err)
    {
        std::cout << glewGetErrorString(err) << std::endl;
    }

    // setup the vao
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
}

cy::Vec3f randomDiskPosition(float radius, float height)
{
    float angle = randFloat(0.0, 360.0); // for some reason math.cos uses degrees
    float s = randFloat(0.0, 1.0);
    float distance = sqrt(s) * radius;
    float y = randFloat(-height / 2, height / 2);
    return cy::Vec3f(distance * cos(angle), y, distance * sin(angle));
}

cy::Vec3f randomPrismPosition(float length, float width, float height)
{
    float x = randFloat(-width / 2, width / 2);
    float y = randFloat(-height / 2, height / 2);
    float z = randFloat(-length / 2, length / 2);
    return cy::Vec3f(x, y, z);
}

void initParticles()
{
    for (int i = 0; i < PARTICLE_COUNT; i++)
    {
        Particle p;
        p.pos = cy::Vec4f(randomDiskPosition(10, 1), 0);
        p.vel = cy::Vec4f(randomPrismPosition(0.5, 0.5, 0.5), 0);
        p.mass = randFloat(MINIMUM_MASS, MAXIMUM_MASS);
        particles.push_back(p);
        particleIndices.push_back(i);
    }

    glGenBuffers(1, &particleIndexBuffer);
    glBindBuffer(GL_ARRAY_BUFFER, particleIndexBuffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(int) * particleIndices.size(), &particleIndices[0], GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    glGenBuffers(1, &particleInputBuffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, particleInputBuffer);
    glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(Particle) * particles.size(), &particles[0], GL_DYNAMIC_COPY);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    glGenBuffers(1, &particleOutputBuffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, particleOutputBuffer);
    glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(Particle) * particles.size(), &particles[0], GL_DYNAMIC_DRAW);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

void initEnv()
{
    std::vector<cy::Vec3f> verts;

    verts.push_back(cy::Vec3f(-1.0f, -1.0f, 0.0f));
    verts.push_back(cy::Vec3f(1.0f, -1.0f, 0.0f));
    verts.push_back(cy::Vec3f(-1.0f, 1.0f, 0.0f));
    verts.push_back(cy::Vec3f(1.0f, 1.0f, 0.0f));

    loadCubeMap(CubemapTexture);

    glGenBuffers(1, &EnvBuffer);
    glBindBuffer(GL_ARRAY_BUFFER, EnvBuffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(cy::Vec3f) * verts.size(), &verts[0], GL_STATIC_DRAW);
}

void loadParticleShader()
{
    std::map<const char *, GLuint *> shaderArgs;
    shaderArgs["viewMatrix"] = &particleMatrixID;
    shaderArgs["rotationMatrix"] = &particleRotationID;
    shaderArgs["maxVelocity"] = &velocityCutoffID;
    loadShaders("shaders\\particle.vert", "shaders\\particle.frag", "shaders\\particle.geom", particleProgramID, shaderArgs);
}

void loadEnvShader()
{
    std::map<const char *, GLuint *> shaderArgs;
    shaderArgs["viewMatrix"] = &EnvMatrixID;
    shaderArgs["colorSampler"] = &EnvTextureID;
    loadShaders("shaders\\env.vert", "shaders\\env.frag", quadProgramID, shaderArgs);
}

void loadGravityShader()
{
    std::map<const char *, GLuint *> shaderArgs;
    loadComputeShader("shaders\\particle.comp", gravityProgramID, shaderArgs);
}

void renderEnvironment()
{
    glDepthMask(GL_FALSE);

    glUseProgram(quadProgramID);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, CubemapTexture);
    glUniform1i(EnvTextureID, 0);

    glUniformMatrix4fv(EnvMatrixID, 1, GL_FALSE, &viewMatrixInverse.cell[0]);

    glEnableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, EnvBuffer);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    glDisableVertexAttribArray(0);
    glDisableVertexAttribArray(1);

    glDepthMask(GL_TRUE);
}

void renderParticles()
{
    glUseProgram(particleProgramID);

    glUniformMatrix4fv(particleMatrixID, 1, GL_FALSE, &viewMatrix.cell[0]);
    cy::Vec3f output = rotation * cy::Vec3f(0.1, 0, 0);
    glUniformMatrix3fv(particleRotationID, 1, GL_FALSE, &rotationInverse.cell[0]);
    glUniform1f(velocityCutoffID, VELOCITY_CUTOFF);

    glEnableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, particleIndexBuffer);
    glVertexAttribIPointer(0, 1, GL_INT, 0, 0);

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, particleOutputBuffer);

    glDrawArrays(GL_POINTS, 0, particles.size());

    glDisableVertexAttribArray(0);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

void runGravity()
{
    glUseProgram(gravityProgramID);

    // swap the buffers
    GLuint tmp = particleInputBuffer;
    particleInputBuffer = particleOutputBuffer;
    particleOutputBuffer = particleInputBuffer;

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, particleInputBuffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, particleOutputBuffer);

    glDispatchCompute(PARTICLE_COUNT, 1, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
}

// The main render loop
void renderLoop()
{
    glClearColor(0.0, 0.0, 0.0, 1.0);
    // glEnable(GL_DEPTH_TEST);
    glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    while (!glfwWindowShouldClose(WINDOW))
    {
        glClearColor(0.0, 0.0, 0.0, 1.0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        if (!paused)
        {
            runGravity();
        }
        renderEnvironment();
        renderParticles();

        glfwSwapBuffers(WINDOW);

        glfwPollEvents();
        usleep(16000);
    }
}

int main(int argc, char *argv[])
{
    initWindow();
    initViewMatrix();
    loadParticleShader();
    loadGravityShader();
    loadEnvShader();
    initParticles();
    initEnv();
    renderLoop();
}