#pragma once

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <iostream>

class FrameBuffer
{
private:
	unsigned int FrameBufferID;

public:
	FrameBuffer(); //constructor
	~FrameBuffer(); //destructor

	void Bind() const;
	void Unbind() const;
}; 