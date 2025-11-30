#include <Dragonfly/editor.h>		 //inlcludes most features
#include <Dragonfly/detail/buffer.h> //will be replaced
#include <Dragonfly/detail/vao.h>	 //will be replaced

#include <glm/glm.hpp>

#include <Dragonfly/detail/Buffer/Buffer.h>

#include <iostream>
#include <fstream>
//#include <string>

#include "application.h"
#include "ImFileDialog/ImFileDialog.h"

#include "performance.h"

//#include <Dragonfly/detail/Spirver/Spirver.h>
#include "Spirv/Spirver_ext.h"
#include <spirv-tools/linker.hpp>

void test1();
void test_transforms();
void raytrace_generation_demo();
void ImFileDialog_Initialize();
void raw_ogl_demo();
void test_double_attach_demo();
std::string getShaderString(const char* _fileName);
void getGenSpirvBinary(const char* _fileName, std::vector<GLuint>& result, Spirver_ext::Stage shader_stage);
void getSpirvBinary(const char* _fileName, std::vector<GLuint>& result, Spirver_ext::Stage shader_stage);
void getSpirvBinarySingle(const char* _fileName, std::vector<GLuint>& result, Spirver_ext::Stage shader_stage);
void combineSpirvBinaries(std::vector<std::vector<GLuint>*> input_bins, std::vector<GLuint>& result);
GLuint loadShader(GLenum _shaderType, const char* _fileName);
GLuint loadShaderSpirv(GLenum _shaderType, const char* _fileName);
GLuint loadShaderSpirv(GLenum _shaderType, const std::vector<GLuint>& rawSpirv);
GLuint createProgram(GLuint vs_ID, GLuint fs_ID);
void drawRawOgl(GLuint programID, GLuint texID, GLuint texLOC, df::VaoArrays& vao);

// Clock/performance measure (needs to be global
std::vector<CSGVMeasurment> measurements;
std::map<std::string, int> measurement_counts;

bool GLOBAL_spirv_precompile = false;

int main(int argc, char* args[])
{
	//main2(argc,args);
	//test_transforms();
	//raytrace_demo();
	raytrace_generation_demo();
	//raw_ogl_demo();
	//test_double_attach_demo();
	return 0;
}

void raytrace_generation_demo()
{
	//std::cout << "OpenGL Version: " << glGetString(GL_VERSION) << std::endl;
	//std::cout << "Use spirv?(1/0): ";
	//std::cin >> use_spirv;
	//std::cout << "\n";
	
	//// Usage modes
	// Opengl (native)
	// Opengl (dragonlfy)
	// Spirv no precompile
	// Spirv skip ASTProgram

	int usage_mode_code = -1;

	std::cout << "Select how the fragment shaders will be compiled:\n";
	std::cout << "1: OpenGL, native\n";
	std::cout << "2: OpenGL, dragonfly framework\n";
	std::cout << "3: Spirv cross compilation\n";
	std::cout << "4: EXPERIMENTAL: Spirv cross compilation, partial pre-compilation (only for testing, breaks on 2nd compilation)\n";
	std::cout << "5: Spirv cross compilation, skip spirv program linkage\n";

	std::string usage_mode_str;
	int usage_mode;

	while (true)
	{
		std::cin >> usage_mode_str;

		if (std::all_of(usage_mode_str.begin(), usage_mode_str.end(), ::isdigit))
		{
			usage_mode = std::stoi(usage_mode_str);
			if (usage_mode >= 1 && usage_mode <= 5)
				break;
		}

		std::cout << "Input must be a number between 1-5, try again:\n";
	}
	
	// Usage mode variables
	//bool use_dragonfly = false;
	//bool use_spirv = true;
	//bool generate_spirv_shaders = true;
	//bool spirv_skipASTprogram = true;
	bool generate_spirv_shaders = false;
	//
	bool use_dragonfly = false;
	bool use_spirv = false;
	bool spirv_skipASTprogram = false;
	GLOBAL_spirv_precompile = false;

	switch (usage_mode)
	{
		case 1: // "1: OpenGL, native\n"
			break;
		case 2: // "2: OpenGL, dragonfly framework\n"
			use_dragonfly = true;
			break;
		case 3: // "3: Spirv cross compilation\n"
			use_spirv = true;
			break;
		case 4: // "4: Spirv cross compilation, partial pre-compilation\n"
			use_spirv = true;
			GLOBAL_spirv_precompile = true;
			break;
		case 5: // "5: Spirv cross compilation, skip spirv program linkage\n"
			use_spirv = true;
			spirv_skipASTprogram = true;
			generate_spirv_shaders = true;
			break;
		default:
			break;
	}

	glm::vec2 iResolution{ 620, 465 };
	df::Sample sam("Ray Tracing Demo", iResolution.x, iResolution.y, df::Sample::FLAGS::DEFAULT);
	// df::Sample simplifies OpenGL, SDL, ImGui, RenderDoc in the render loop, and handles user input via callback member functions in priority queues
	df::Camera cam;								// Implements a camera event class with handles
	sam.AddHandlerClass(cam, 5);				// class callbacks will be called to change its state
	sam.AddHandlerClass<df::ImGuiHandler>(10);	// static handle functions only

	eltecg::ogl::ArrayBuffer MyVBO;	MyVBO.constructMutable(std::vector<glm::vec2>{ {-1, -1}, { 3, -1 }, { -1, 3 }}, GL_STATIC_DRAW);
	eltecg::ogl::VertexArray MyVAO;	MyVAO.addVBO<glm::vec2>(MyVBO);
	df::VaoArrays demoVao((GLuint)MyVAO, GL_TRIANGLE_STRIP, 3, 0u); // temporary solution that wraps an ID

	df::TextureCube<> testCubemap("Assets/xpos.png", "Assets/xneg.png", "Assets/ypos.png", "Assets/yneg.png", "Assets/zpos.png", "Assets/zneg.png");
	df::Texture2D<> testTex = testCubemap[df::X_POS]; // 2D view of a cubemap face

	df::ShaderProgramEditorVF* raytraceProgram;

	cam.SetSpeed(10.0);
	//cam.LookAt(glm::vec3(12, 8, 8));
	cam.LookAt(glm::vec3(4, 4, -3));

	auto begin = std::chrono::high_resolution_clock::now();
	// Set time just to decide auto
	auto bef_gen_time = std::chrono::high_resolution_clock::now();

	// Spirv mode specific variables
	GLuint m_spirv_programID;
	GLuint m_spirv_vsID;
	GLuint m_spirv_fsID;
	GLuint m_gen_fsID;
	// Spirv uniform locations
	GLuint m_loc_iResolution;
	GLuint m_loc_iTime;
	GLuint m_loc_cameraAt;
	GLuint m_loc_cameraPos;
	GLuint m_loc_cameraView;
	// Texture uniform locations
	GLuint m_loc_texImg;

	// Renderloop variables
	bool recompile = false;
	int recompile_count = 0;
	bool camera_changed;

	if (!use_spirv && use_dragonfly)
	{
		raytraceProgram = new df::ShaderProgramEditorVF("Ray tracing demo shader program");
		*raytraceProgram << "Shaders/raytrace.vert"_vert << "Shaders/raytrace.frag"_frag << df::LinkProgram;
	}
	else
	{
		if (use_spirv)
		{
			Spirver_ext::Init();
			if (generate_spirv_shaders)
			{
				std::vector<GLuint> spirv_bin_vs;
				std::vector<GLuint> spirv_bin_fs;
				if (spirv_skipASTprogram)
				{
					InsertMeasurement("Start compiling and loading spirv binaries from glsl (vertex, no ASTProgram)");
					getSpirvBinarySingle("Shaders/raytrace.vert", spirv_bin_vs, Spirver_ext::Stage::Vertex);
					InsertMeasurement("Compiled spirv binaries (vertex, no ASTProgram)");
					m_spirv_vsID = loadShaderSpirv(GL_VERTEX_SHADER, spirv_bin_vs);
					InsertMeasurement("Compiled and loaded spirv binaries (vertex, no ASTProgram)");

					getSpirvBinarySingle("Shaders/raytrace.frag", spirv_bin_fs, Spirver_ext::Stage::Fragment);
					InsertMeasurement("Compiled spirv binaries (fragment, no ASTProgram)");
					m_spirv_fsID = loadShaderSpirv(GL_FRAGMENT_SHADER, spirv_bin_fs);
					InsertMeasurement("Compiled and loaded spirv binaries (fragment, no ASTProgram)");
				}
				else
				{
					InsertMeasurement("Start compiling and loading spirv binaries from glsl (vertex)");
					getSpirvBinary("Shaders/raytrace.vert", spirv_bin_vs, Spirver_ext::Stage::Vertex);
					InsertMeasurement("Compiled spirv binaries (vertex)");
					m_spirv_vsID = loadShaderSpirv(GL_VERTEX_SHADER, spirv_bin_vs);
					InsertMeasurement("Compiled and loaded spirv binaries (vertex)");

					getSpirvBinary("Shaders/raytrace.frag", spirv_bin_fs, Spirver_ext::Stage::Fragment);
					InsertMeasurement("Compiled spirv binaries (fragment)");
					m_spirv_fsID = loadShaderSpirv(GL_FRAGMENT_SHADER, spirv_bin_fs);
					InsertMeasurement("Compiled and loaded spirv binaries (fragment)");
				}
			}
			else
			{
				InsertMeasurement("Start loading spirv binaries from file");
				m_spirv_vsID = loadShaderSpirv(GL_VERTEX_SHADER, "Shaders/raytrace.vert.spv");
				//m_spirv_fsID = loadShaderSpirv(GL_FRAGMENT_SHADER, "Shaders/raytrace.frag.spv");
				m_spirv_fsID = loadShaderSpirv(GL_FRAGMENT_SHADER, "Shaders/raytrace.joint.frag.spv");
				InsertMeasurement("Loaded spirv binaries from file");
			}

			m_spirv_programID = createProgram(m_spirv_vsID, m_spirv_fsID);
			InsertMeasurement("Finished creating program");

			// Precompile everything but the SDF(model) for faster real-time generation
			Spirver_ext::precompileGlslAsAstShader(getShaderString("Shaders/raytrace_joint_template.frag"),
				Spirver_ext::Stage::Fragment);
			InsertMeasurement("Precompiled most shadercode used for generation", true);
		}
		else
		{
			InsertMeasurement("Start loading glsl shaders");
			m_spirv_vsID = loadShader(GL_VERTEX_SHADER, "Shaders/raytrace.vert");
			m_spirv_fsID = loadShader(GL_FRAGMENT_SHADER, "Shaders/raytrace.frag");
			InsertMeasurement("Loaded glsl shaders");

			m_spirv_programID = createProgram(m_spirv_vsID, m_spirv_fsID);
			InsertMeasurement("Finished creating program", true);
		}

		m_loc_iResolution = glGetUniformLocation(m_spirv_programID, "iResolution");
		m_loc_iTime = glGetUniformLocation(m_spirv_programID, "iTime");
		m_loc_cameraAt = glGetUniformLocation(m_spirv_programID, "cameraAt");
		m_loc_cameraPos = glGetUniformLocation(m_spirv_programID, "cameraPos");
		m_loc_cameraView = glGetUniformLocation(m_spirv_programID, "cameraView");

		m_loc_texImg = glGetUniformLocation(m_spirv_programID, "texImg");
	}

	GL_CHECK; //extra opengl error checking in GPU Debug build configuration

	Application_Initialize();
	ImFileDialog_Initialize();

	sam.Run([&](float deltaTime) //delta time in ms
		{
			camera_changed = cam.Update();

			// Clock stuff
			auto end = std::chrono::high_resolution_clock::now();
			auto dur = end - begin;
			float ms = std::chrono::duration_cast<std::chrono::milliseconds>(dur).count() / 1000.0;
			
			// Recompile value of previous frame, runs if last frame recompiled
			if (recompile)
			{
				auto aft_gen_time = std::chrono::high_resolution_clock::now();
				auto gen_dur = aft_gen_time - bef_gen_time;
				auto gen_dur_ms = std::chrono::duration_cast<std::chrono::milliseconds>(gen_dur).count();
				std::cout << "Compilation #" << (++recompile_count) << " took :" << gen_dur_ms << "ms\n\n";
			}
			
			//measurements.emplace_back(CSGVMeasurment(std::chrono::high_resolution_clock::now(), "asdqwe"));

			if (GLOBAL_spirv_precompile)
			{
				Spirver_ext::precompileGlslAsAstShader(getShaderString("Shaders/raytrace_joint_template.frag"),
					Spirver_ext::Stage::Fragment);
			}

			bef_gen_time = std::chrono::high_resolution_clock::now();
			recompile = Application_Frame();
			if (recompile)
			{
				if (!use_spirv && use_dragonfly)
				{
					delete raytraceProgram;
					raytraceProgram = new df::ShaderProgramEditorVF("Ray tracing demo shader program");
					*raytraceProgram << "Shaders/raytrace.vert"_vert << "Shaders/gen_raytrace.frag"_frag << df::LinkProgram;
				}
				else
				{
					glDeleteShader(m_gen_fsID);
					glDeleteProgram(m_spirv_programID);
					
					if (use_spirv)
					{
						//std::vector<GLuint> spirv_bin_template_fs;
						//std::vector<GLuint> spirv_bin_sdf_fs;
						//std::vector<GLuint> spirv_bin_combined_fs;
						////
						//// Does not work unfortunatelly, a valid shader can be compiled with this command,
						//// but seems like chunks cant
						////
						////getSpirvBinarySingle("Shaders/raytrace_joint_template.frag", spirv_bin_template_fs, Spirver_ext::Stage::Fragment);
						//// This can compile chunks, but needs entrypoint
						////getSpirvBinary("Shaders/raytrace_sdf_linktest.frag", spirv_bin_sdf_fs, Spirver_ext::Stage::Fragment);						
						//std::vector<std::vector<GLuint>*> spirv_bins = {&spirv_bin_template_fs, &spirv_bin_sdf_fs};
						//
						//combineSpirvBinaries(spirv_bins, spirv_bin_combined_fs);

						//m_gen_fsID = loadShaderSpirv(GL_FRAGMENT_SHADER, spirv_bin_combined_fs);
						// or
						// m_gen_fsID = loadShaderSpirv(GL_FRAGMENT_SHADER, "Shaders/gen_raytrace.spv");

						//
						// Lets try 2 TShader*s
						//
						//TShader* alreadycompiled saved somewhere
						
						std::vector<GLuint> spirv_bin_gen_fs;
						//getGenSpirvBinary("Shaders/raytrace_sdf_linktest.frag", spirv_bin_gen_fs, Spirver_ext::Stage::Fragment);

						if (GLOBAL_spirv_precompile)
						{
							getGenSpirvBinary("Shaders/gen_raytrace.frag", spirv_bin_gen_fs, Spirver_ext::Stage::Fragment);
						}
						else if (spirv_skipASTprogram)
						{
							getSpirvBinarySingle("Shaders/gen_raytrace.frag", spirv_bin_gen_fs, Spirver_ext::Stage::Fragment);
						}
						else
						{
							getSpirvBinary("Shaders/gen_raytrace.frag", spirv_bin_gen_fs, Spirver_ext::Stage::Fragment);
						}
						
						m_gen_fsID = loadShaderSpirv(GL_FRAGMENT_SHADER, spirv_bin_gen_fs);
					}
					else
					{
						m_gen_fsID = loadShader(GL_FRAGMENT_SHADER, "Shaders/gen_raytrace.frag");
					}

					m_spirv_programID = createProgram(m_spirv_vsID, m_gen_fsID);
				}
			}
			// Update resolution in case the window is resized
			//iResolution = df::Backbuffer.getSize();
			if (camera_changed)
				iResolution = cam.GetSize();

			if (!use_spirv && use_dragonfly)
			{
				df::Backbuffer << df::Clear() << *raytraceProgram << "iResolution" << iResolution
					<< "iTime" << ms
					<< "texImg" << testTex
					<< "cameraAt" << cam.GetAt()
					<< "cameraPos" << cam.GetEye()
					<< "cameraView" << cam.GetView();
				*raytraceProgram << demoVao;
				GL_CHECK;
				raytraceProgram->Render(); // Draws Imgui window for editing the corresponding glsl shaders
			}
			else
			{
				glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
				glUseProgram(m_spirv_programID);
				// Set uniforms
				glUniform1f(m_loc_iTime, ms);
				glUniform2fv(m_loc_iResolution, 1, &iResolution.x);
				glUniform3fv(m_loc_cameraAt, 1, &cam.GetAt().x);
				glUniform3fv(m_loc_cameraPos, 1, &cam.GetEye().x);
				glUniformMatrix3fv(m_loc_cameraView, 1, GL_FALSE, &cam.GetView()[0][0]);

				drawRawOgl(m_spirv_programID, (GLuint)testTex, m_loc_texImg, demoVao);
			}
		}
	);

	Application_Finalize();

	if (use_spirv)
		Spirver_ext::Clean();

	if (use_spirv || !use_dragonfly)
	{
		glDeleteShader(m_spirv_vsID);
		glDeleteShader(m_spirv_fsID);
		glDeleteShader(m_gen_fsID);
		glDeleteProgram(m_spirv_programID);
	}
}

void ImFileDialog_Initialize()
{
	ifd::FileDialog::Instance().CreateTexture = [](uint8_t* data, int w, int h, char fmt) -> void* {
		GLuint tex;

		glGenTextures(1, &tex);
		glBindTexture(GL_TEXTURE_2D, tex);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, (fmt == 0) ? GL_BGRA : GL_RGBA, GL_UNSIGNED_BYTE, data);
		glGenerateMipmap(GL_TEXTURE_2D);
		glBindTexture(GL_TEXTURE_2D, 0);

		return (void*)tex;
	};
	ifd::FileDialog::Instance().DeleteTexture = [](void* tex) {
		GLuint texID = (GLuint)tex;
		glDeleteTextures(1, &texID);
	};
}

void test_transforms()
{
	df::Sample sam("Dragonfly Demo", 1024, 768, df::Sample::FLAGS::DEFAULT);
	// df::Sample simplifies OpenGL, SDL, ImGui, RenderDoc in the render loop, and handles user input via callback member functions in priority queues
	df::Camera cam;								// Implements a camera event class with handles
	sam.AddHandlerClass(cam, 5);				// class callbacks will be called to change its state
	sam.AddHandlerClass<df::ImGuiHandler>(10);	// static handle functions only

	eltecg::ogl::ArrayBuffer MyVBO;	MyVBO.constructMutable(std::vector<glm::vec2>{ {-1, -1}, { 1, -1 }, { 0, 1 }}, GL_STATIC_DRAW);
	eltecg::ogl::VertexArray MyVAO;	MyVAO.addVBO<glm::vec2>(MyVBO);
	df::VaoArrays demoVao((GLuint)MyVAO, GL_TRIANGLE_STRIP, 3, 0u); // temporary solution that wraps an ID

	df::ShaderProgramEditorVF testProgram = "Transformations-test shader program";
	testProgram << "Shaders/test_transforms.vert"_vert << "Shaders/test_transforms.frag"_frag << df::LinkProgram;

	GL_CHECK; //extra opengl error checking in GPU Debug build configuration

	sam.Run([&](float deltaTime) //delta time in ms
		{
			cam.Update();

			glm::mat4 mvp;
			df::Backbuffer << df::Clear() << testProgram << "mvp" << mvp;
			testProgram << demoVao; // Rendering a pixel shader

			GL_CHECK;
			testProgram.Render();
		}
	);
}

void test1()
{
	df::Sample sam("Dragonfly Demo", 1024, 768, df::Sample::FLAGS::DEFAULT);
	// df::Sample simplifies OpenGL, SDL, ImGui, RenderDoc in the render loop, and handles user input via callback member functions in priority queues
	df::Camera cam;								// Implements a camera event class with handles
	sam.AddHandlerClass(cam, 5);				// class callbacks will be called to change its state
	sam.AddHandlerClass<df::ImGuiHandler>(10);	// static handle functions only

	eltecg::ogl::ArrayBuffer MyVBO;	MyVBO.constructMutable(std::vector<glm::vec2>{ {-1, -1}, { 1, -1 }, { 0, 1 }}, GL_STATIC_DRAW);
	eltecg::ogl::VertexArray MyVAO;	MyVAO.addVBO<glm::vec2>(MyVBO);
	df::VaoArrays demoVao((GLuint)MyVAO, GL_TRIANGLE_STRIP, 3, 0u); // temporary solution that wraps an ID

	df::ShaderProgramEditorVF testProgram = "Test shader program";
	testProgram << "Shaders/test.vert"_vert << "Shaders/test.frag"_frag << df::LinkProgram;

	GL_CHECK; //extra opengl error checking in GPU Debug build configuration

	sam.Run([&](float deltaTime) //delta time in ms
		{


			cam.Update();
			df::Backbuffer << df::Clear() << testProgram;
			testProgram << demoVao; // Rendering a pixel shader

			GL_CHECK;
			testProgram.Render();
		}
	);
}

int main2(int argc, char* args[])
{
	df::Sample sam("Dragonfly Demo", 720, 480, df::Sample::FLAGS::DEFAULT);
	// df::Sample simplifies OpenGL, SDL, ImGui, RenderDoc in the render loop, and handles user input via callback member functions in priority queues
	df::Camera cam;								// Implements a camera event class with handles
	sam.AddHandlerClass(cam, 5);				// class callbacks will be called to change its state
	sam.AddHandlerClass<df::ImGuiHandler>(10);	// static handle functions only

	eltecg::ogl::ArrayBuffer MyVBO;	MyVBO.constructMutable(std::vector<glm::vec2>{ {-1, -1}, { 1, -1 }, { 0, 1 }}, GL_STATIC_DRAW);
	eltecg::ogl::VertexArray MyVAO;	MyVAO.addVBO<glm::vec2>(MyVBO);		//these two classes will be removed from Dragonfly as soon as we have the replacement ready

	df::VaoArrays demoVao((GLuint)MyVAO, GL_TRIANGLE_STRIP, 3, 0u); // temporary solution that wraps an ID

	df::TextureCube<> testCubemap("Assets/xpos.png", "Assets/xneg.png", "Assets/ypos.png", "Assets/yneg.png", "Assets/zpos.png", "Assets/zneg.png");
	df::Texture2D<> testTex = testCubemap[df::X_POS]; // 2D view of a cubemap face

	df::ShaderProgramEditorVF program = "MyShaderProgram";
	program << "Shaders/vert.vert"_vert << "Shaders/frag.frag"_frag << df::LinkProgram;
	df::ShaderProgramEditorVF postprocess = "Postprocess shader program";
	postprocess << "Shaders/postprocess.vert"_vert << "Shaders/postprocess.frag"_frag << df::LinkProgram;

	int w = df::Backbuffer.getWidth(), h = df::Backbuffer.getHeight();
	auto frameBuff = df::Renderbuffer<df::depth24>(w, h) + df::Texture2D<>(w, h, 1);

	sam.AddResize([&](int w, int h) {frameBuff = frameBuff.MakeResized(w, h); });

	GL_CHECK; //extra opengl error checking in GPU Debug build configuration

	sam.Run([&](float deltaTime) //delta time in ms
		{
			cam.Update();

			frameBuff << df::Clear() << program << "texImg" << testTex;
			program << demoVao;	//Rendering: Ensures that both the vao and program is attached

			df::Backbuffer << df::Clear() << postprocess << "texFrame" << frameBuff.get<glm::u8vec3>();
			postprocess << df::NoVao(GL_TRIANGLES, 3); // Rendering a pixel shader

			GL_CHECK;
			program.Render(); postprocess.Render(); //only the UI!!
		}
	);
	return 0;
}

void raw_ogl_demo()
{
	glm::vec2 iResolution{ 620, 465 };
	df::Sample sam("Ray Tracing Demo", iResolution.x, iResolution.y, df::Sample::FLAGS::DEFAULT);
	// df::Sample simplifies OpenGL, SDL, ImGui, RenderDoc in the render loop, and handles user input via callback member functions in priority queues
	df::Camera cam;								// Implements a camera event class with handles
	sam.AddHandlerClass(cam, 5);				// class callbacks will be called to change its state
	sam.AddHandlerClass<df::ImGuiHandler>(10);	// static handle functions only

	eltecg::ogl::ArrayBuffer MyVBO;	MyVBO.constructMutable(std::vector<glm::vec2>{ {-1, -1}, { 3, -1 }, { -1, 3 }}, GL_STATIC_DRAW);
	eltecg::ogl::VertexArray MyVAO;	MyVAO.addVBO<glm::vec2>(MyVBO);
	df::VaoArrays demoVao((GLuint)MyVAO, GL_TRIANGLE_STRIP, 3, 0u); // temporary solution that wraps an ID

	df::TextureCube<> testCubemap("Assets/xpos.png", "Assets/xneg.png", "Assets/ypos.png", "Assets/yneg.png", "Assets/zpos.png", "Assets/zneg.png");
	df::Texture2D<> testTex = testCubemap[df::X_POS]; // 2D view of a cubemap face


	// SPIRV demo specific code
	GLuint vs_ID = loadShader(GL_VERTEX_SHADER, "Shaders/vert.vert");
	GLuint fs_ID = loadShader(GL_FRAGMENT_SHADER, "Shaders/frag.frag");

	GLuint m_programID = createProgram(vs_ID,fs_ID);

	GLuint m_loc_tex = glGetUniformLocation(m_programID, "texImg");


	GL_CHECK; //extra opengl error checking in GPU Debug build configuration

	cam.SetSpeed(10.0);
	cam.LookAt(glm::vec3(4, 4, -3));

	sam.Run([&](float deltaTime) //delta time in ms
		{
			cam.Update();
			// ? glViewport SOMEWHERE (in bind buffer), need?
			//glViewport(0, 0, 620, 465);

			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
			glUseProgram(m_programID);

			drawRawOgl(m_programID,(GLuint)testTex,m_loc_tex,demoVao);

			// i dont do:
			// set framebuffer (optional prob)
			// bind and clear buffers
			//df::Backbuffer << df::Clear() << *raytraceProgram << "texImg" << testTex;
			//*raytraceProgram << demoVao;
			//// REPLACE THIS
			//df::Backbuffer << df::Clear() << *raytraceProgram << "iResolution" << iResolution
			//	<< "iTime" << ms
			//	<< "texImg" << testTex
			//	<< "cameraAt" << cam.GetAt()
			//	<< "cameraPos" << cam.GetEye()
			//	<< "cameraView" << cam.GetView();
			//*raytraceProgram << demoVao;
		}
	);
	// will delete after glDeleteProgram only
	glDeleteShader(vs_ID);
	glDeleteShader(fs_ID);
	glDeleteProgram(m_programID);
}

void test_double_attach_demo()
{
	glm::vec2 iResolution{ 620, 465 };
	df::Sample sam("Ray Tracing Demo", iResolution.x, iResolution.y, df::Sample::FLAGS::DEFAULT);
	// df::Sample simplifies OpenGL, SDL, ImGui, RenderDoc in the render loop, and handles user input via callback member functions in priority queues
	df::Camera cam;								// Implements a camera event class with handles
	sam.AddHandlerClass(cam, 5);				// class callbacks will be called to change its state
	sam.AddHandlerClass<df::ImGuiHandler>(10);	// static handle functions only

	eltecg::ogl::ArrayBuffer MyVBO;	MyVBO.constructMutable(std::vector<glm::vec2>{ {-1, -1}, { 3, -1 }, { -1, 3 }}, GL_STATIC_DRAW);
	eltecg::ogl::VertexArray MyVAO;	MyVAO.addVBO<glm::vec2>(MyVBO);
	df::VaoArrays demoVao((GLuint)MyVAO, GL_TRIANGLE_STRIP, 3, 0u); // temporary solution that wraps an ID

	df::TextureCube<> testCubemap("Assets/xpos.png", "Assets/xneg.png", "Assets/ypos.png", "Assets/yneg.png", "Assets/zpos.png", "Assets/zneg.png");
	df::Texture2D<> testTex = testCubemap[df::X_POS]; // 2D view of a cubemap face

	df::ShaderProgramEditorVF* raytraceProgram;

	cam.SetSpeed(10.0);
	//cam.LookAt(glm::vec3(12, 8, 8));
	cam.LookAt(glm::vec3(4, 4, -3));

	auto begin = std::chrono::high_resolution_clock::now();
	// Set time just to decide auto lol #DELETE
	auto bef_gen_time = std::chrono::high_resolution_clock::now();

	Application_Initialize();
	ImFileDialog_Initialize();

	bool use_spirv = true;
	bool recompile = false;
	int recompile_count = 0;
	// Camera related variables
	bool camera_changed;

	// Spirv mode specific variables
	GLuint m_spirv_programID;
	GLuint m_spirv_vsID;
	GLuint m_spirv_fsID;
	GLuint m_gen_fsID;
	// Spirv uniform locations
	GLuint m_loc_iResolution;
	GLuint m_loc_iTime;
	GLuint m_loc_cameraAt;
	GLuint m_loc_cameraPos;
	GLuint m_loc_cameraView;
	// Texture uniform locations
	GLuint m_loc_texImg;


	std::cout << "Double attach declaration test!\n";

	
	GLuint test_fs_1;
	GLuint test_fs_2;
	if (use_spirv)
	{
		// This test will never work for the following reason:
		// Source: "Linking SPIR-V" chapter @"https://www.khronos.org/opengl/wiki/SPIR-V"
		// "Also, note that SPIR-V shaders must have an entry-point.
		// So SPIR-V modules for the same stage cannot be linked together.
		// Each SPIR-V shader object must provide all of the code for its module.
		// Sadge
		// Either spirv-dis a complete fs.spv and edit the sdf value in manually
		// then spirv-as it. (lot of work, cant just copy the sdf into it, but
		// maybe i can just dump function info by glslangtospv functions.)
		// OR
		// Keep 2 TShaders, 1 precompiled, 1 compile on demand, link both on demand.
		// Not sure if it will be fast or not.
		// OR
		// Use linker SOMEHOW
		// OR
		// Use compile-only option trough API somehow, or generate spirv with non api version
		// but idk why or how useful
		//
		Spirver_ext::Init();
		std::vector<GLuint> spirv_bin_fs_render;
		std::vector<GLuint> spirv_bin_fs_sdf;
		getSpirvBinary("Shaders/raytrace_joint_template.frag", spirv_bin_fs_render, Spirver_ext::Stage::Fragment);
		getSpirvBinary("Shaders/raytrace_sdf_linktest.frag", spirv_bin_fs_sdf, Spirver_ext::Stage::Fragment);

		m_spirv_vsID = loadShaderSpirv(GL_VERTEX_SHADER, "Shaders/raytrace.vert.spv");
		test_fs_1 = loadShaderSpirv(GL_FRAGMENT_SHADER, spirv_bin_fs_render);
		test_fs_2 = loadShaderSpirv(GL_FRAGMENT_SHADER, spirv_bin_fs_sdf);
	}
	else
	{
		m_spirv_vsID = loadShader(GL_VERTEX_SHADER, "Shaders/raytrace.vert");
		test_fs_1 = loadShader(GL_FRAGMENT_SHADER, "Shaders/raytrace_joint_template.frag");
		test_fs_2 = loadShader(GL_FRAGMENT_SHADER, "Shaders/raytrace_sdf_linktest.frag");
	}

	GLuint m_programID = glCreateProgram();

	glAttachShader(m_programID, m_spirv_vsID);
	glAttachShader(m_programID, test_fs_1);
	glAttachShader(m_programID, test_fs_2);

	// IMPORTANT! Assign atribs before linking
	glBindAttribLocation(m_programID,
		0,				// VAO channel index
		"vs_in_pos");	// shader variable name

	glLinkProgram(m_programID);

	GLint infoLogLength = 0, result = 0;
	glGetProgramiv(m_programID, GL_LINK_STATUS, &result);
	glGetProgramiv(m_programID, GL_INFO_LOG_LENGTH, &infoLogLength);
	if (GL_FALSE == result || infoLogLength != 0)
	{
		std::vector<char> VertexShaderErrorMessage(infoLogLength);
		glGetProgramInfoLog(m_programID, infoLogLength, nullptr, VertexShaderErrorMessage.data());
		std::cerr << "[glLinkProgram] Shader linking error:\n" << &VertexShaderErrorMessage[0] << std::endl;
	}
	m_spirv_programID = m_programID;


	m_loc_iResolution = glGetUniformLocation(m_spirv_programID, "iResolution");
	m_loc_iTime = glGetUniformLocation(m_spirv_programID, "iTime");
	m_loc_cameraAt = glGetUniformLocation(m_spirv_programID, "cameraAt");
	m_loc_cameraPos = glGetUniformLocation(m_spirv_programID, "cameraPos");
	m_loc_cameraView = glGetUniformLocation(m_spirv_programID, "cameraView");

	m_loc_texImg = glGetUniformLocation(m_spirv_programID, "texImg");

	GL_CHECK; //extra opengl error checking in GPU Debug build configuration

	sam.Run([&](float deltaTime) //delta time in ms
		{
			camera_changed = cam.Update();

			// Clock stuff
			auto end = std::chrono::high_resolution_clock::now();
			auto dur = end - begin;
			float ms = std::chrono::duration_cast<std::chrono::milliseconds>(dur).count() / 1000.0;

			// Update resolution in case the window is resized
			//iResolution = df::Backbuffer.getSize();
			if (camera_changed)
				iResolution = cam.GetSize();

			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
			glUseProgram(m_spirv_programID);
			// Set uniforms
			glUniform1f(m_loc_iTime, ms);
			glUniform2fv(m_loc_iResolution, 1, &iResolution.x);
			glUniform3fv(m_loc_cameraAt, 1, &cam.GetAt().x);
			glUniform3fv(m_loc_cameraPos, 1, &cam.GetEye().x);
			glUniformMatrix3fv(m_loc_cameraView, 1, GL_FALSE, &cam.GetView()[0][0]);

			drawRawOgl(m_spirv_programID, (GLuint)testTex, m_loc_texImg, demoVao);
		}
	);
	glDeleteShader(m_spirv_vsID);
	glDeleteShader(test_fs_1);
	glDeleteShader(test_fs_2);

	glDeleteProgram(m_spirv_programID);
	Spirver_ext::Clean();
}

std::string getShaderString(const char* _fileName)
{
	std::string shaderCode = "";

	std::ifstream shaderStream(_fileName);
	if (!shaderStream.is_open())
	{
		std::cerr << "[std::ifstream] Error during the reading of " << _fileName << " shaderfile's source!\n";
		return "";
	}

	std::string line = "";
	while (std::getline(shaderStream, line))
	{
		shaderCode += line + "\n";
	}
	shaderStream.close();

	return shaderCode;
}

void getGenSpirvBinary(const char* _fileName, std::vector<GLuint>& result, Spirver_ext::Stage shader_stage)
{
	if (Spirver_ext::glslToSpirvAddPrecompiled(getShaderString(_fileName), result, shader_stage))
		std::cout << "Successfully converted " << _fileName << " and other precompiled shaders to spirv binary!\n";
	else
		std::cout << "Failed to convert " << _fileName << " and other precompiled shaders to spirv binary!\n";
}

void getSpirvBinary(const char* _fileName, std::vector<GLuint>& result, Spirver_ext::Stage shader_stage)
{
	if (Spirver_ext::glslToSpirv(getShaderString(_fileName), result, shader_stage))
		std::cout << "Successfully converted " << _fileName << " to spirv binary!\n";
	else
		std::cout << "Failed to convert " << _fileName << " to spirv binary!\n";
}

void getSpirvBinarySingle(const char* _fileName, std::vector<GLuint>& result, Spirver_ext::Stage shader_stage)
{
	if (Spirver_ext::singleGlslToSpirv(getShaderString(_fileName), result, shader_stage))
		std::cout << "Successfully converted " << _fileName << " to spirv binary! (skipped ASTProgram)\n";
	else
		std::cout << "Failed to convert " << _fileName << " to spirv binary!\n";
}

void combineSpirvBinaries(std::vector<std::vector<GLuint>*> input_bins, std::vector<GLuint>& result)
{

}

GLuint loadShader(GLenum _shaderType, const char* _fileName)
{
	// shader azonosito letrehozasa
	GLuint loadedShader = glCreateShader(_shaderType);

	// ha nem sikerult hibauzenet es -1 visszaadasa
	if (loadedShader == 0)
	{
		std::cerr << "[glCreateShader] Error during the initialization of shader: " << _fileName << "!\n";
		return 0;
	}

	// shaderkod betoltese _fileName fajlbol
	std::string shaderCode = "";

	// _fileName megnyitasa
	std::ifstream shaderStream(_fileName);

	if (!shaderStream.is_open())
	{
		std::cerr << "[std::ifstream] Error during the reading of " << _fileName << " shaderfile's source!\n";
		return 0;
	}

	// file tartalmanak betoltese a shaderCode string-be
	std::string line = "";
	while (std::getline(shaderStream, line))
	{
		shaderCode += line + "\n";
	}

	shaderStream.close();

	// fajlbol betoltott kod hozzarendelese a shader-hez
	const char* sourcePointer = shaderCode.c_str();
	glShaderSource(loadedShader, 1, &sourcePointer, nullptr);

	// shader leforditasa
	glCompileShader(loadedShader);

	// ellenorizzuk, hogy sikerult-e a forditas
	GLint result = GL_FALSE;
	int infoLogLength;

	// forditas statuszanak lekerdezese
	glGetShaderiv(loadedShader, GL_COMPILE_STATUS, &result);
	glGetShaderiv(loadedShader, GL_INFO_LOG_LENGTH, &infoLogLength);

	if (GL_FALSE == result)
	{
		// hibauzenet elkerese es kiirasa
		std::vector<char> ShaderErrorMessage(infoLogLength);
		glGetShaderInfoLog(loadedShader, infoLogLength, nullptr, &ShaderErrorMessage[0]);

		std::cerr << "[glCompileShader] Shader compilation error in " << _fileName << ":\n" << &ShaderErrorMessage[0] << std::endl;
	}

	return loadedShader;
}

GLuint loadShaderSpirv(GLenum _shaderType, const char* _fileName)
{
	// shader azonosito letrehozasa
	GLuint loadedShader = glCreateShader(_shaderType);

	// ha nem sikerult hibauzenet es -1 visszaadasa
	if (loadedShader == 0)
	{
		std::cerr << "[glCreateShader] Error during the initialization of shader: " << _fileName << "!\n";
		return 0;
	}

	std::ifstream shaderStream(_fileName, std::ios::binary);
	// Check if the file is successfully opened
	if (!shaderStream.is_open())
	{
		std::cerr << "[std::ifstream] Error during the reading of " << _fileName << " shaderfile's source!\n";
		return 0;
	}
	// Get the file size
	shaderStream.seekg(0, std::ios::end);
	std::streamsize fileSize = shaderStream.tellg();
	shaderStream.seekg(0, std::ios::beg);

	// Create a vector to store the data
	std::vector<unsigned char> rawSpirv(fileSize);

	// Read data from file into vector
	if (!shaderStream.read(reinterpret_cast<char*>(rawSpirv.data()), fileSize)) {
		// Handle error, maybe throw an exception
		std::cerr << "Failed to read binary data from file: " << _fileName;
		return 0;
	}
	// Close the file
	shaderStream.close();

	// Apply the shader binary SPIR-V code to the shader object.
	glShaderBinary(1, &loadedShader, GL_SHADER_BINARY_FORMAT_SPIR_V, rawSpirv.data(), rawSpirv.size() * sizeof(unsigned char));

	// Specialize the shader, no specialization constants in our case
	std::string vsEntrypoint = "main"; // Get VS entry point name
	glSpecializeShader(loadedShader, (const GLchar*)vsEntrypoint.c_str(), 0, nullptr, nullptr);

	// ellenorizzuk, hogy sikerult-e a forditas
	GLint result = GL_FALSE;
	int infoLogLength;

	// forditas statuszanak lekerdezese
	glGetShaderiv(loadedShader, GL_COMPILE_STATUS, &result);
	glGetShaderiv(loadedShader, GL_INFO_LOG_LENGTH, &infoLogLength);

	if (GL_FALSE == result)
	{
		// hibauzenet elkerese es kiirasa
		std::vector<char> ShaderErrorMessage(infoLogLength);
		glGetShaderInfoLog(loadedShader, infoLogLength, nullptr, &ShaderErrorMessage[0]);

		std::cerr << "[glCompileShader] Shader compilation error in " << _fileName << ":\n" << &ShaderErrorMessage[0] << std::endl;
	}

	return loadedShader;
}

GLuint loadShaderSpirv(GLenum _shaderType, const std::vector<GLuint>& rawSpirv)
{
	// shader azonosito letrehozasa
	GLuint loadedShader = glCreateShader(_shaderType);

	// ha nem sikerult hibauzenet es -1 visszaadasa
	if (loadedShader == 0)
	{
		std::cerr << "[glCreateShader] Error during the initialization of shader from binary in memory data!\n";
		return 0;
	}

	// Apply the shader binary SPIR-V code to the shader object.
	glShaderBinary(1, &loadedShader, GL_SHADER_BINARY_FORMAT_SPIR_V, rawSpirv.data(), rawSpirv.size() * sizeof(GLuint));

	// Specialize the shader, no specialization constants in our case
	std::string vsEntrypoint = "main"; // Get VS entry point name
	glSpecializeShader(loadedShader, (const GLchar*)vsEntrypoint.c_str(), 0, nullptr, nullptr);

	// ellenorizzuk, hogy sikerult-e a forditas
	GLint result = GL_FALSE;
	int infoLogLength;

	// forditas statuszanak lekerdezese
	glGetShaderiv(loadedShader, GL_COMPILE_STATUS, &result);
	glGetShaderiv(loadedShader, GL_INFO_LOG_LENGTH, &infoLogLength);

	if (GL_FALSE == result)
	{
		// hibauzenet elkerese es kiirasa
		std::vector<char> ShaderErrorMessage(infoLogLength);
		glGetShaderInfoLog(loadedShader, infoLogLength, nullptr, &ShaderErrorMessage[0]);

		std::cerr << "[glCompileShader] Shader compilation error when compiling SPIRV shader from binary in memory data!\n"
			<< &ShaderErrorMessage[0] << std::endl;
	}

	return loadedShader;
}

GLuint createProgram(GLuint vs_ID, GLuint fs_ID)
{
	GLuint m_programID = glCreateProgram();

	glAttachShader(m_programID, vs_ID);
	glAttachShader(m_programID, fs_ID);

	// IMPORTANT! Assign atribs before linking
	glBindAttribLocation(m_programID,
		0,				// VAO channel index
		"vs_in_pos");	// shader variable name

	glLinkProgram(m_programID);

	GLint infoLogLength = 0, result = 0;
	glGetProgramiv(m_programID, GL_LINK_STATUS, &result);
	glGetProgramiv(m_programID, GL_INFO_LOG_LENGTH, &infoLogLength);
	if (GL_FALSE == result || infoLogLength != 0)
	{
		std::vector<char> VertexShaderErrorMessage(infoLogLength);
		glGetProgramInfoLog(m_programID, infoLogLength, nullptr, VertexShaderErrorMessage.data());
		std::cerr << "[glLinkProgram] Shader linking error:\n" << &VertexShaderErrorMessage[0] << std::endl;
	}

	return m_programID;
}

void drawRawOgl(GLuint programID, GLuint texID, GLuint texLOC, df::VaoArrays& vao)
{
	// set uniforms/textures
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, texID);
	glUniform1i(texLOC, 0);

	// use VAO
	glBindVertexArray(vao._id);
	glDrawArrays(vao._mode, vao._first, vao._count);

	//unbind
	glBindVertexArray(0);
	glUseProgram(0);
}