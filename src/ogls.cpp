#include "ogls.h"

#include <stdio.h>
#include <glad/glad.h>

struct OglsVertexBuffer
{
	float* vertices;
	uint32_t id, size, count;
	GLenum bufferMode;
};

struct OglsIndexBuffer
{
	uint32_t* indices;
	uint32_t id, size, count;
	GLenum bufferMode;
};

struct OglsVertexArray
{
	uint32_t id, vboId, iboId;
};

struct OglsShader
{
	uint32_t id;
};

namespace ogls
{
	static GLenum getOglDataTypeEnum(OglsDataType dataType);
	static GLenum getBufferMode(OglsBufferMode bufferMode);

	static GLenum getOglDataTypeEnum(OglsDataType dataType)
	{
		switch (dataType)
		{
		case Ogls_DataType_Byte:          { return GL_BYTE; }
		case Ogls_DataType_UnsignedByte:  { return GL_UNSIGNED_BYTE; }
		case Ogls_DataType_Short:         { return GL_SHORT; }
		case Ogls_DataType_UnsignedShort: { return GL_UNSIGNED_SHORT; }
		case Ogls_DataType_Int:           { return GL_INT; }
		case Ogls_DataType_UnsignedInt:   { return GL_UNSIGNED_INT; }
		case Ogls_DataType_Float:         { return GL_FLOAT; }
		case Ogls_DataType_Double:        { return GL_DOUBLE; }
		}

		return GL_NONE;
	}
	
	static GLenum getBufferMode(OglsBufferMode bufferMode)
	{
		switch (bufferMode)
		{
		case Ogls_BufferMode_Static: { return GL_STATIC_DRAW; }
		case Ogls_BufferMode_Dynamic: { return GL_DYNAMIC_DRAW; }
		}

		return GL_STATIC_DRAW;
	}

	OglsResult printErrorCodeMsg(const char* file, int line)
	{
		GLenum err;
		while((err = glGetError()) != GL_NO_ERROR)
		{
			switch (err)
			{
			case GL_INVALID_ENUM: { printf("%s", "ogl error: invalid enum value"); break; }
			case GL_INVALID_VALUE: { printf("%s", "ogl error: invalid parameter value"); break; }
			case GL_INVALID_INDEX: { printf("%s", "ogl error: invalid operation, state for a command is invalid for its given parameters"); break; }
			case GL_STACK_OVERFLOW: { printf("%s", "ogl error: stack overflow, stack pushing operation causes stack overflow"); }
			case GL_STACK_UNDERFLOW: { printf("%s", "ogl error: stack underflow, stach popping operation occurs while stack is at its lowest point"); break; }
			case GL_OUT_OF_MEMORY: { printf("%s", "ogl error: out of memory, memory allocation cannot allocate enough memory"); break; }
			case GL_INVALID_FRAMEBUFFER_OPERATION: { printf("%s", "ogl error: reading or writing to a frambuffer is not complete"); break; }
			}
		}

		return err == 0 ? Ogls_Result_Success : Ogls_Result_Failed;
	}

	OglsResult createVertexBuffer(OglsVertexBuffer** vertexBuffer, float* vertices, uint32_t size, OglsBufferMode bufferMode)
	{
		GLenum vertexBufferMode = getBufferMode(bufferMode);

		uint32_t vbo;
		glGenBuffers(1, &vbo);
		glBindBuffer(GL_ARRAY_BUFFER, vbo);
		glBufferData(GL_ARRAY_BUFFER, size, vertices, vertexBufferMode);
		if (OGLS_CHECK_ERROR() == Ogls_Result_Failed) { return Ogls_Result_Failed; }
		glBindBuffer(GL_ARRAY_BUFFER, 0);

		*vertexBuffer = new OglsVertexBuffer();
		OglsVertexBuffer* vertexBufferPtr = *vertexBuffer;
		vertexBufferPtr->vertices = vertices;
		vertexBufferPtr->count = size / sizeof(float);
		vertexBufferPtr->size = size;
		vertexBufferPtr->id = vbo;
		vertexBufferPtr->bufferMode = vertexBufferMode;

		return Ogls_Result_Success;
	}

	OglsResult createIndexBuffer(OglsIndexBuffer** indexBuffer, uint32_t* indices, uint32_t size, OglsBufferMode bufferMode)
	{
		GLenum indexBufferMode = getBufferMode(bufferMode);

		uint32_t ibo;
		glGenBuffers(1, &ibo);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, size, indices, indexBufferMode);
		if (OGLS_CHECK_ERROR() == Ogls_Result_Failed) { return Ogls_Result_Failed; }
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

		*indexBuffer = new OglsIndexBuffer();
		OglsIndexBuffer* indexBufferPtr = *indexBuffer;
		indexBufferPtr->indices = indices;
		indexBufferPtr->count = size / sizeof(uint32_t);
		indexBufferPtr->size = size;
		indexBufferPtr->id = ibo;
		indexBufferPtr->bufferMode = indexBufferMode;

		return Ogls_Result_Success;
	}

	OglsResult createVertexArray(OglsVertexArray** vertexArray, OglsVertexArrayCreateInfo* createInfo)
	{
		uint32_t vao;
		glGenVertexArrays(1, &vao);
		glBindVertexArray(vao);
		
		glBindBuffer(GL_ARRAY_BUFFER, createInfo->vertexBuffer->id);
		glBufferData(GL_ARRAY_BUFFER, createInfo->vertexBuffer->size, createInfo->vertexBuffer->vertices, createInfo->vertexBuffer->bufferMode);
		if (OGLS_CHECK_ERROR() == Ogls_Result_Failed) { return Ogls_Result_Failed; }
	
		if (createInfo->indexBuffer)
		{
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, createInfo->indexBuffer->id);
			glBufferData(GL_ELEMENT_ARRAY_BUFFER, createInfo->indexBuffer->size, createInfo->indexBuffer->indices, createInfo->indexBuffer->bufferMode);
			if (OGLS_CHECK_ERROR() == Ogls_Result_Failed) { return Ogls_Result_Failed; }
		}

		for (uint32_t i = 0; i < createInfo->attributeCount; i++)
		{ 
			glEnableVertexAttribArray(createInfo->pAttributes[i].index);
			glVertexAttribPointer(
				createInfo->pAttributes[i].index,
				createInfo->pAttributes[i].components,
				getOglDataTypeEnum(createInfo->pAttributes[i].dataType),
				GL_FALSE,
				createInfo->pAttributes[i].stride,
				createInfo->pAttributes[i].offset);
		}

		glBindVertexArray(0);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

		*vertexArray = new OglsVertexArray();
		OglsVertexArray* vertexArrayPtr = *vertexArray;
		vertexArrayPtr->vboId = createInfo->vertexBuffer->id;
		if (createInfo->indexBuffer) vertexArrayPtr->iboId = createInfo->indexBuffer->id;
		vertexArrayPtr->id = vao;

		return Ogls_Result_Success;
	}

	OglsResult createShaderFromStr(OglsShader** shader, OglsShaderCreateInfo* shaderStrings)
	{
		uint32_t vertexShader, fragmentShader;

		vertexShader = glCreateShader(GL_VERTEX_SHADER);
		glShaderSource(vertexShader, 1, &shaderStrings->vertexSrc, NULL);
		glCompileShader(vertexShader);

		fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
		glShaderSource(fragmentShader, 1, &shaderStrings->fragmentSrc, NULL);
		glCompileShader(fragmentShader);

		uint32_t shaderProgram;
		shaderProgram = glCreateProgram();
		glAttachShader(shaderProgram, vertexShader);
		glAttachShader(shaderProgram, fragmentShader);
		glLinkProgram(shaderProgram);

		glDeleteShader(vertexShader);
		glDeleteShader(fragmentShader);

		*shader = new OglsShader();
		OglsShader* shaderPtr = *shader;
		shaderPtr->id = shaderProgram;

		return Ogls_Result_Success;
	}


	float* getVertexBufferVertices(OglsVertexBuffer* vertexBuffer)
	{
		return vertexBuffer->vertices;
	}

	uint32_t getVertexBufferCount(OglsVertexBuffer* vertexBuffer)
	{
		return vertexBuffer->count;
	}

	uint32_t getVertexBufferSize(OglsVertexBuffer* vertexBuffer)
	{
		return vertexBuffer->size;
	}

	uint32_t getVertexBufferId(OglsVertexBuffer* vertexBuffer)
	{
		return vertexBuffer->id;
	}

	uint32_t* getIndexBufferIndices(OglsIndexBuffer* indexBuffer)
	{
		return indexBuffer->indices;
	}

	uint32_t getIndexBufferCount(OglsIndexBuffer* indexBuffer)
	{
		return indexBuffer->count;
	}

	uint32_t getIndexBufferSize(OglsIndexBuffer* indexBuffer)
	{
		return indexBuffer->size;
	}

	uint32_t getVertexBufferId(OglsIndexBuffer* indexBuffer)
	{
		return indexBuffer->id;
	}

	uint32_t getVertexArrayId(OglsVertexArray* vertexArray)
	{
		return vertexArray->id;
	}

	uint32_t getShaderId(OglsShader* shader)
	{
		return shader->id;
	}


	void bindVertexBuffer(OglsVertexBuffer* vertexBuffer)
	{
		if (!vertexBuffer)
		{
			glBindBuffer(GL_ARRAY_BUFFER, 0);
			return;
		}

		glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer->id);
	}

	void bindIndexBuffer(OglsIndexBuffer* indexBuffer)
	{
		if (!indexBuffer)
		{
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
			return;
		}

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuffer->id);
	}

	void bindVertexArray(OglsVertexArray* vertexArray)
	{
		if (!vertexArray)
		{
			glBindVertexArray(0);
			return;
		}

		glBindVertexArray(vertexArray->id);
	}

	void bindShader(OglsShader* shader)
	{
		glUseProgram(shader->id);
	}

	void bindVertexBufferSubData(OglsVertexBuffer* vertexBuffer, uint32_t size, uint32_t offset, float* data)
	{
		glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer->id);
		glBufferSubData(GL_ARRAY_BUFFER, offset, size, data);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
	}
	
	void bindIndexBufferSubData(OglsIndexBuffer* indexBuffer, uint32_t size, uint32_t offset, uint32_t* data)
	{
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuffer->id);
		glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, offset, size, data);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	}

	void destroyVertexBuffer(OglsVertexBuffer* vertexBuffer)
	{
		glDeleteBuffers(1, &vertexBuffer->id);
		delete vertexBuffer;
	}

	void destroyIndexBuffer(OglsIndexBuffer* indexBuffer)
	{
		glDeleteBuffers(1, &indexBuffer->id);
		delete indexBuffer;
	}

	void destroyVertexArray(OglsVertexArray* vertexArray)
	{
		glDeleteVertexArrays(1, &vertexArray->id);
		delete vertexArray;
	}

	void destroyShader(OglsShader* shader)
	{
		glDeleteShader(shader->id);
		delete shader;
	}

	void renderDraw(uint32_t first, uint32_t count)
	{
		glDrawArrays(GL_TRIANGLES, first, count);
	}

	void renderDrawIndex(uint32_t count)
	{
		glDrawElements(GL_TRIANGLES, count, GL_UNSIGNED_INT, 0);
	}

	void renderDrawMode(uint32_t mode, uint32_t first, uint32_t count)
	{
		glDrawArrays(mode, first, count);
	}
	
	void renderDrawIndexMode(uint32_t mode, uint32_t count)
	{
		glDrawElements(mode, count, GL_UNSIGNED_INT, 0);
	}
}
