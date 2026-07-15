#pragma once

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <iostream>

class IndexBuffer
{
private:
	unsigned int IndexBufferID; //The reference

public:
	IndexBuffer(const void* data, unsigned int count);
	~IndexBuffer();

	void Bind() const;
	void Unbind() const;
};
#pragma once
