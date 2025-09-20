//
// ** Based on Dragonfly/detail/Spirver. **
// Majority of the code is the same, with extended functinality for
// compiling and dynamic linking several spirv modules
//
#pragma once

#include <string>
#include <GL/glew.h>
#include <glslang/Include/ResourceLimits.h>
#include <glslang/Public/ShaderLang.h>
#include <spirv-tools/optimizer.hpp>
#include <glslang/SPIRV/SpvTools.h>
#include <sstream>
#include <iostream>

#include <Dragonfly/detail/Shader/ShaderSource.h>

/// Compile, convert and analyze shaders with static functions
class Spirver_ext
{
public:
	/// GLSL shader stage 
	enum class Stage { Vertex, Fragment, Geometry, TessellationControl, TessellationEvaluation, Compute };
	/// Spirver::Stage to string
	const static char* stageNames[];
	/// Number of elements in Spirver::Stage
	const static int stageCount;

	//
	// Extensions:
	//
	static std::vector<glslang::TShader*> precompiled_shaders;

	static GLuint StageToGlsl(Stage stage);
	static EShLanguage StageToGlslang(Stage stage);
	static int StageToInt(Stage stage);
	static Stage StageToSpirver(GLenum stage);
	static Stage StageToSpirver(EShLanguage stage);
	static Stage StageToSpirver(int i);
	static Stage StageToSpirver(const std::string& s);
	static Stage StageToSpirver(df::detail::SHADER_TYPE stage);
	static GLuint StageToGlsl(EShLanguage stage) { return StageToGlsl(StageToSpirver(stage)); }
	static GLuint StageToGlsl(int i) { return StageToGlsl(StageToSpirver(i)); }


	/// Get all error messages and clear stored errors
	static std::string GetErrors();


	static void Init();
	static void Clean();


	/* compilation */

	/// Auto uniform locations
	static bool glslToSpirv(const std::string& glsl, std::vector<GLuint>& spirv, Stage stage, int uniformLocBase)
	{
		return glslToSpirv(glsl, spirv, stage, uniformLocBase, false);
	}
	/// Explicit uniform locations
	static bool glslToSpirv(const std::string& glsl, std::vector<GLuint>& spirv, Stage stage)
	{
		return glslToSpirv(glsl, spirv, stage, -1);
	}
	/// Auto uniform locations
	static bool glslToSpirv(const df::detail::GeneratedCode& glsl, std::vector<GLuint>& spirv, Stage stage, int uniformLocBase)
	{
		df::detail::GeneratedCode::ConcatenatedCode conc = glsl.Concatenate();
		return glslToSpirv(conc.code, spirv, stage, uniformLocBase, false);
	}


	/* optimization */
	// Exclude this functionality for now
	//template<typename T>
	//static bool optimizeSpirv(std::vector<T>& spirv);

	//
	// Extensions:
	//
	static bool precompileGlslAsAstShader(const std::string& glsl, Stage stage, int uniformBase = -1);

	static bool glslToSpirvAddPrecompiled(const std::string& glsl, std::vector<GLuint>& spirv, Stage stage, int uniformBase);
	static bool glslToSpirvAddPrecompiled(const std::string& glsl, std::vector<GLuint>& spirv, Stage stage)
	{
		return glslToSpirvAddPrecompiled(glsl, spirv, stage, -1);
	}
	static bool singleGlslToSpirv(const std::string& glsl, std::vector<GLuint>& spirv, Stage stage, int uniformBase);
	static bool singleGlslToSpirv(const std::string& glsl, std::vector<GLuint>& spirv, Stage stage)
	{
		return singleGlslToSpirv(glsl, spirv, stage, -1);
	}	

private:
	const static TBuiltInResource DefaultTBuiltInResource;

	// culmulative error messages
	static std::stringstream errors;
	enum class LogType { Program = 0, Shader = 1, PrespecShader = 2 };
	const static char* logTypeStr[];
	// returns false on error, true on no error
	static bool printLog(GLuint program, LogType logType);
	static bool printLog(glslang::TShader* object);
	static bool printLog(glslang::TProgram* object);
	static bool printLog(spv::SpvBuildLogger& object);
	static void printSpirvOptLog(spv_message_level_t level, const char* source, const spv_position_t& position, const char* msg);

	static bool glslToAstShader(const std::string& source, glslang::TShader* shader, int uniformBase = -1);
	static bool astShaderToAstProgram(glslang::TShader* shader, glslang::TProgram* program);
	static bool astProgramToSpirv(glslang::TProgram* program, std::vector<GLuint>& spirv, Stage stage);

	static bool glslToSpirv(const std::string& glsl, std::vector<GLuint>& spirv, Stage stage, int uniformBase, bool analyze);
	// Extensions:
	static bool singleAstShaderToSpirv(glslang::TShader* shader, std::vector<GLuint>& spirv, Stage stage);
};

//#include <Dragonfly/detail/Spirver/Spirver.inl>