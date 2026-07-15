#include "Shader.h"

Shader::Shader(const std::string& VertexSource, const std::string& TessellationControlSource, const std::string& TessellationEvalSource, const std::string& GeometrySource, const std::string& FragmentSource)
{
	ShaderID = glCreateProgram();
	const char* vSource = VertexSource.c_str();
	const char* gSource = GeometrySource.c_str();
	const char* fSource = FragmentSource.c_str();
	const char* tControlSource = TessellationControlSource.c_str();
	const char* tEvalSource = TessellationEvalSource.c_str();

	unsigned int vertexShader = glCreateShader(GL_VERTEX_SHADER);
	unsigned int geometryShader = 0;
	unsigned int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
	unsigned int tessellationControlShader = 0;
	unsigned int tessellationEvalShader = 0;

	glShaderSource(vertexShader, 1, &vSource, NULL);
	glShaderSource(fragmentShader, 1, &fSource, NULL);

	glCompileShader(vertexShader);
	glCompileShader(fragmentShader);

	glAttachShader(ShaderID, vertexShader);
	glAttachShader(ShaderID, fragmentShader);

	if (!GeometrySource.empty())
	{
		geometryShader = glCreateShader(GL_GEOMETRY_SHADER);
		glShaderSource(geometryShader, 1, &gSource, NULL);
		glCompileShader(geometryShader);
		glAttachShader(ShaderID, geometryShader);
	}

	if (!TessellationControlSource.empty())
	{
		tessellationControlShader = glCreateShader(GL_TESS_CONTROL_SHADER);
		glShaderSource(tessellationControlShader, 1, &tControlSource, NULL);
		glCompileShader(tessellationControlShader);
		glAttachShader(ShaderID, tessellationControlShader);
	}

	if (!TessellationEvalSource.empty())
	{
		tessellationEvalShader = glCreateShader(GL_TESS_EVALUATION_SHADER);
		glShaderSource(tessellationEvalShader, 1, &tEvalSource, NULL);
		glCompileShader(tessellationEvalShader);
		glAttachShader(ShaderID, tessellationEvalShader);
	}

	glLinkProgram(ShaderID);
	glValidateProgram(ShaderID);

	ErrorHandling(vertexShader, 0);  //ITS NOT WORKING D:
	//include geometry error handling
	ErrorHandling(tessellationControlShader, 1);
	ErrorHandling(tessellationEvalShader, 2);
	ErrorHandling(geometryShader, 3);
	ErrorHandling(fragmentShader, 4);


	glDeleteShader(vertexShader);
	glDeleteShader(fragmentShader);

	if (geometryShader)
		glDeleteShader(geometryShader);

	if (tessellationControlShader)
		glDeleteShader(tessellationControlShader);

	if (tessellationEvalShader)
		glDeleteShader(tessellationEvalShader);
}

Shader::~Shader()
{
	glDeleteProgram(ShaderID);
}

void Shader::Bind() const
{
	glUseProgram(ShaderID);
}

void Shader::Unbind() const
{
	glUseProgram(0);
}

void Shader::SetUniform1i(const std::string& name, int v0)
{
	glUniform1i(GetUniformLocation(name), v0);
}

void Shader::SetUniform1f(const std::string& name, float v0)
{
	glUniform1f(GetUniformLocation(name), v0);
}

void Shader::SetUniform3f(const std::string& name, float v0, float v1, float v2)
{
	glUniform3f(GetUniformLocation(name), v0, v1, v2);
}


void Shader::SetUniform4f(const std::string& name, float v0, float v1, float v2, float v3)
{
	glUniform4f(GetUniformLocation(name), v0, v1, v2, v3);
}

void Shader::SetUniform3v(const std::string& name, glm::vec3 v0)
{
	glUniform3f(GetUniformLocation(name), v0[0], v0[1], v0[2]);
}

void Shader::SetUniform4v(const std::string& name, glm::vec3 v0, float v4)
{
	glUniform4f(GetUniformLocation(name), v0[0], v0[1], v0[2], v4);
}


void Shader::SetUniformMat4f(const std::string& name, const glm::mat4& matrix)
{
	glUniformMatrix4fv(GetUniformLocation(name), 1, GL_FALSE, &matrix[0][0]);
}


int Shader::GetUniformLocation(const std::string& name)
{
	int location = glGetUniformLocation(ShaderID, name.c_str());
	if (location == -1)
		std::cout << "WARNING: Uniform " << name << " doesn't exist :(" << std::endl;

	return location;
}

void Shader::ErrorHandling(unsigned int shader, int count) //Checks for shader compilation errors.
{
	int success;
	char infoLog[512];
	std::string shaderType;

	if (count == 0)
		shaderType.append("Vertex");

	else if (count == 1)
		shaderType.append("Tesselation Control");

	else if (count == 2)
		shaderType.append("Tessellation Eval");

	else if (count == 3)
		shaderType.append("Geometry");

	else if (count == 4)
		shaderType.append("Fragment");

	glGetShaderiv(shader, GL_COMPILE_STATUS, &success);

	if (!success)
	{
		glGetShaderInfoLog(shader, 512, NULL, infoLog);
		std::cout << "ERROR: " << shaderType << " compilation failed.\n" << infoLog << std::endl;
	}
}