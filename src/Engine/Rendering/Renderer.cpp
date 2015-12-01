#include "Rendering/Renderer.h"

void Renderer::Initialize()
{
	InitializeWindow();

	// Create default camera
	m_DefaultCamera = new ::Camera((float)m_Resolution.Width / m_Resolution.Height, glm::radians(45.f), 0.01f, 5000.f);
	m_DefaultCamera->SetPosition(glm::vec3(0, 0, 0));
	if (m_Camera == nullptr) {
		m_Camera = m_DefaultCamera;
	}

	glfwSwapInterval(m_VSYNC);

	InitializeShaders();
}

void Renderer::InitializeWindow()
{
	// Initialize GLFW
	if (!glfwInit()) {
		LOG_ERROR("GLFW: Initialization failed");
		exit(EXIT_FAILURE);
	}

	// Create a window
	GLFWmonitor* monitor = nullptr;
	if (m_Fullscreen) {
		monitor = glfwGetPrimaryMonitor();
	}
	//glfwWindowHint(GLFW_SAMPLES, 8);
	m_Window = glfwCreateWindow(m_Resolution.Width, m_Resolution.Height, "daydream", monitor, nullptr);
	if (!m_Window) {
		LOG_ERROR("GLFW: Failed to create window");
		exit(EXIT_FAILURE);
	}
	glfwMakeContextCurrent(m_Window);

	// GL version info
	glGetIntegerv(GL_MAJOR_VERSION, &m_GLVersion[0]);
	glGetIntegerv(GL_MINOR_VERSION, &m_GLVersion[1]);
	m_GLVendor = (GLchar*)glGetString(GL_VENDOR);
	std::stringstream ss;
	ss << m_GLVendor << " OpenGL " << m_GLVersion[0] << "." << m_GLVersion[1];
#ifdef DEBUG
	ss << " DEBUG";
#endif
	LOG_INFO(ss.str().c_str());
	glfwSetWindowTitle(m_Window, ss.str().c_str());

	// Initialize GLEW
	if (glewInit() != GLEW_OK) {
		LOG_ERROR("GLEW: Initialization failed");
		exit(EXIT_FAILURE);
	}
}

void Renderer::InitializeShaders()
{
	m_BasicForwardProgram.AddShader(std::shared_ptr<Shader>(new VertexShader("Shaders/BasicForward.vert.glsl")));
	m_BasicForwardProgram.AddShader(std::shared_ptr<Shader>(new FragmentShader("Shaders/BasicForward.frag.glsl")));
	m_BasicForwardProgram.Compile();
	m_BasicForwardProgram.Link();
}

void Renderer::Draw(RenderQueueCollection& rq)
{
	glClearColor(255.f / 255, 163.f / 255, 176.f / 255, 0.f);
	glClear(GL_COLOR_BUFFER_BIT);
	glfwSwapBuffers(m_Window);
}

