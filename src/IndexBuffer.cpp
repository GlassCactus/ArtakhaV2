#include "IndexBuffer.h"

IndexBuffer::IndexBuffer(const void* data, unsigned int size)
{
	glGenBuffers(1, &IndexBufferID);// Generates buffer
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, IndexBufferID);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, size, data, GL_DYNAMIC_DRAW);
}

IndexBuffer::~IndexBuffer()
{
	glDeleteBuffers(1, &IndexBufferID);
}

void IndexBuffer::Bind() const
{
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, IndexBufferID);
}

void IndexBuffer::Unbind() const
{
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}