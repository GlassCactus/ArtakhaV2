#pragma once

#include "VertexBuffer.h"
#include "VertexBufferLayout.h"
#include "IndexBuffer.h"

class VertexArray
{
private:
	unsigned int VertexArrayID;

public:
	VertexArray();
	~VertexArray();

	void AddBuffer(const VertexBufferLayout& layout);

	void Bind() const;
	void Unbind() const;
};
