#include "FrameBuffer.h"

FrameBuffer::FrameBuffer()
{
	glGenFramebuffers(1, &FrameBufferID);
	glBindBuffer(GL_FRAMEBUFFER, FrameBufferID);
}

FrameBuffer::~FrameBuffer()
{
	glDeleteFramebuffers(1, &FrameBufferID);
}

void FrameBuffer::Bind() const
{
	glBindFramebuffer(GL_FRAMEBUFFER, FrameBufferID);
}

void FrameBuffer::Unbind() const
{
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

