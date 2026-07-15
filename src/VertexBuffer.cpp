#include "VertexBuffer.h"

VertexBuffer::VertexBuffer(const void* data, unsigned int size)
{
	glGenBuffers(1, &VertexBufferID); // Generates buffer
	glBindBuffer(GL_ARRAY_BUFFER, VertexBufferID); //We're selecting the "layer" to "draw" on like in photoshop. This is what binding means.
	glBufferData(GL_ARRAY_BUFFER, size, data, GL_DYNAMIC_DRAW); //Copies all the data onto the buffers
}


VertexBuffer::~VertexBuffer()
{
	glDeleteBuffers(1, &VertexBufferID);
}


void VertexBuffer::Bind() const
{
	glBindBuffer(GL_ARRAY_BUFFER, VertexBufferID);
}


void VertexBuffer::Unbind() const
{
	glBindBuffer(GL_ARRAY_BUFFER, 0);
}