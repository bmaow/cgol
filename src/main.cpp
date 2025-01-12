#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <glad/glad.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <cstdio>
#include <cmath>
#include <cstdint>
#include <vector>
#include <chrono>
#include <string>

#include <imgui/imgui.h>
#include <imgui/imgui_impl_glfw.h>
#include <imgui/imgui_impl_opengl3.h>

#include "ogls.h"


#define COLOR_FG 0.78, 0.82, 1.0
#define COLOR_FG2 0.098, 0.094, 0.156
#define COLOR_BG 0.12, 0.11, 0.18
#define COLOR_RED 0.97, 0.46, 0.55
#define CELL_SPACE_WIDTH 120
#define CELL_SPACE_HEIGHT 120
#define CELL_SPACE_SCALE 13.0f


#define PI (22.0f/7.0f) /* 3.1415... */
#define ARRAY_LEN(x) (sizeof(x) / sizeof(x[0]))

static const uint32_t s_MaxVertices = UINT16_MAX * 4;
static const uint32_t s_MaxIndices = s_MaxVertices * 6;

class Timer
{
private:
	std::chrono::time_point<std::chrono::high_resolution_clock> m_Time;
	float m_Timef;
	bool m_Pause;

public:
	void start() { m_Time = std::chrono::high_resolution_clock::now(); }
	void reset() { m_Timef = 0.0f; m_Time = std::chrono::high_resolution_clock::now(); }
	void pause() { if (m_Pause) return; m_Pause = true; m_Timef += std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now() - m_Time).count() * 0.001f * 0.001f * 0.001f; }
	void play()  { if (!m_Pause) return; m_Pause = false; start(); }
	float elapsed() { if (m_Pause) { return m_Timef; } pause(); play(); return m_Timef; }
	float elapsedms() { return elapsed() * 1000.0f; }
};

struct Vertex
{
	OglsVec2 pos;
	OglsVec3 color;
};

struct DrawCommand
{
	float* pVertices;
	uint32_t* pIndices;
	uint32_t vertexCount;
	uint32_t vertexSize;
	uint32_t vertexAttributeCount;
	uint32_t indexCount;
};

class DrawList
{
	std::vector<float> m_VerticesRaw;
	std::vector<uint32_t> m_Indices;
	std::vector<DrawCommand> m_DrawCommands;
	size_t m_Vertices;

public:
	void push_back(const DrawCommand& drawCmd)
	{
		m_DrawCommands.push_back(drawCmd);
		
		m_Indices.insert(m_Indices.end(), drawCmd.pIndices, drawCmd.pIndices + drawCmd.indexCount);
		for (uint32_t i = m_Indices.size() - drawCmd.indexCount; i < m_Indices.size(); i++)
		{
			m_Indices[i] += m_Vertices;
		}

		m_VerticesRaw.insert(m_VerticesRaw.end(), drawCmd.pVertices, drawCmd.pVertices + drawCmd.vertexCount * drawCmd.vertexAttributeCount);
		m_Vertices += drawCmd.vertexCount;
	}
	void clear()
	{
		m_DrawCommands.clear();
		m_VerticesRaw.clear();
		m_Indices.clear();
		m_Vertices = 0;
	}
	bool empty()
	{
		return m_VerticesRaw.empty() && m_Indices.empty() && m_DrawCommands.empty();
	}

	float* vertices()                         { return m_VerticesRaw.data(); }
	const float* vertices() const             { return m_VerticesRaw.data(); }
	size_t vertex_count()                     { return m_VerticesRaw.size(); }
	size_t vertex_size()                      { return m_VerticesRaw.size() * sizeof(float); }

	uint32_t* indices()                       { return m_Indices.data(); }
	const uint32_t* indices() const           { return m_Indices.data(); }
	size_t index_count()                      { return m_Indices.size(); }
	size_t index_size()                       { return m_Indices.size() * sizeof(uint32_t); }

	DrawCommand* drawcmds()                { return m_DrawCommands.data(); }
	const DrawCommand* drawcmds() const    { return m_DrawCommands.data(); }
	size_t drawcmd_count()                    { return m_DrawCommands.size(); }
};

struct BatchGroup
{
	DrawList list;
	OglsVertexBuffer* vertexBuffer;
	OglsIndexBuffer* indexBuffer;
	OglsVertexArray* vertexArray;
};

const char* vertexShaderSource = R"(
#version 330 core

layout (location = 0) in vec2 aPos;
layout (location = 1) in vec3 aColor;

out vec3 fragColor;

uniform mat4 u_Camera;

void main()
{
    gl_Position = u_Camera * vec4(aPos, 0.0, 1.0);
	fragColor = aColor;
}
)";

const char* fragmentShaderSource = R"(
#version 330 core

in vec3 fragColor;

out vec4 outColor;

void main()
{
    outColor = vec4(fragColor, 1.0f);
}
)";

void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    glViewport(0, 0, width, height);
}

float radians(float deg)
{
	return (deg * PI * 0.005555f);
}

void cameraMovement(GLFWwindow* window, float* x, float* y, float dt)
{
	float speed = 1000.0f;

	if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS)
	{
		speed *= 10.0f;
	}

	if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
	{
		*x += speed * dt;
	}
	if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
	{
		*x -= speed * dt;
	}
	if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
	{
		*y += speed * dt;
	}
	if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
	{
		*y -= speed * dt;
	}
}

void cameraScale(GLFWwindow* window, float* scale, float dt)
{
	float speed = 2.0f;

	if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS)
	{
		speed *= 5.0f;
	}

	if (glfwGetKey(window, GLFW_KEY_MINUS) == GLFW_PRESS)
	{
		*scale += speed * dt;
	}
	if (glfwGetKey(window, GLFW_KEY_EQUAL) == GLFW_PRESS)
	{
		*scale -= speed * dt;
	}

	if (*scale < 0.5f)
		*scale = 0.5f;
}

void drawRect(BatchGroup* batch, OglsVec2 pos, OglsVec2 size, OglsVec3 color)
{
	float vertices[] =
	{
		pos.x, pos.y + size.y,          color.r, color.g, color.b,
		pos.x, pos.y,                   color.r, color.g, color.b,
		pos.x + size.x, pos.y,          color.r, color.g, color.b,
		pos.x + size.x, pos.y + size.y, color.r, color.g, color.b,
	};

	uint32_t indices[] =
	{
		0, 1, 2,
		0, 2, 3,
	};

	DrawCommand drawCmd{};
	drawCmd.pVertices = vertices;
	drawCmd.vertexCount = 4;
	drawCmd.vertexSize = sizeof(float);
	drawCmd.vertexAttributeCount = 5;
	drawCmd.pIndices = indices;
	drawCmd.indexCount = ARRAY_LEN(indices);

	batch->list.push_back(drawCmd);
}

void submitDrawList(BatchGroup* batch)
{
	ogls::bindVertexBufferSubData(batch->vertexBuffer, batch->list.vertex_size(), 0, batch->list.vertices());
	ogls::bindIndexBufferSubData(batch->indexBuffer, batch->list.index_size(), 0, batch->list.indices());

	ogls::bindVertexArray(batch->vertexArray);
	ogls::renderDrawIndex(batch->list.index_count());
	ogls::bindVertexArray(0);
}

void clearDrawList(BatchGroup* batch)
{
	batch->list.clear();
}

void drawLine(BatchGroup* batch, OglsVec2 pos1, OglsVec2 pos2, OglsVec3 color)
{
	float vertices[] =
	{
		pos1.x, pos1.y, color.r, color.g, color.b,
		pos2.x, pos2.y, color.r, color.g, color.b,
	};

	uint32_t indices[] =
	{
		0, 1
	};

	DrawCommand drawCmd{};
	drawCmd.pVertices = vertices;
	drawCmd.vertexCount = 2;
	drawCmd.vertexSize = sizeof(float);
	drawCmd.vertexAttributeCount = 5;
	drawCmd.pIndices = indices;
	drawCmd.indexCount = ARRAY_LEN(indices);

	batch->list.push_back(drawCmd);
}

void drawRectImmediate(BatchGroup* batch, OglsVec2 pos, OglsVec2 size, OglsVec3 color)
{
	batch->list.clear();
	drawRect(batch, pos, size, color);

	ogls::bindVertexBufferSubData(batch->vertexBuffer, batch->list.vertex_size(), 0, batch->list.vertices());
	ogls::bindIndexBufferSubData(batch->indexBuffer, batch->list.index_size(), 0, batch->list.indices());

	ogls::bindVertexArray(batch->vertexArray);
	ogls::renderDrawIndex(batch->list.index_count());
	ogls::bindVertexArray(0);
}

struct Cell
{
	int x, y;
	bool alive;
};

int main(int argv, char** argc)
{
	if (!glfwInit())
	{
		printf("failed to initialize glfw\n");
		return -1;
	}

	printf("glfw initialized\n");

	GLFWwindow* window = glfwCreateWindow(1280, 800, "conway game of life", NULL, NULL);
	if (!window)
	{
		printf("failed to create window!\n");
		glfwTerminate();
		return -1;
	}

	glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwMakeContextCurrent(window);

	if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
	{
		printf("failed to initialize glad!\n");
		return -1;
	}

	printf("%s\n", "glad initialized\n");

	// Setup Dear ImGui context
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

	ImGui_ImplGlfw_InitForOpenGL(window, true);
	ImGui_ImplOpenGL3_Init("#version 330 core");


	// setup opengl buffers
	std::vector<OglsVertexArrayAttribute> attributePtrs =
	{
		{ 0, 2, sizeof(Vertex), Ogls_DataType_Float, (void*)0 },
		{ 1, 3, sizeof(Vertex), Ogls_DataType_Float, (void*)(2 * sizeof(float)) },
	};

	OglsVertexBuffer* vertexBuffer;
	ogls::createVertexBuffer(&vertexBuffer, nullptr, sizeof(Vertex) * s_MaxVertices, Ogls_BufferMode_Dynamic);

	OglsIndexBuffer* indexBuffer;
	ogls::createIndexBuffer(&indexBuffer, nullptr, sizeof(uint32_t) * s_MaxIndices, Ogls_BufferMode_Dynamic);

	OglsVertexArrayCreateInfo vertexArrayCreatInfo{};
	vertexArrayCreatInfo.vertexBuffer = vertexBuffer;
	vertexArrayCreatInfo.indexBuffer = indexBuffer;
	vertexArrayCreatInfo.pAttributes = attributePtrs.data();
	vertexArrayCreatInfo.attributeCount = attributePtrs.size();

	OglsVertexArray* vertexArray;
	ogls::createVertexArray(&vertexArray, &vertexArrayCreatInfo);



	// setup opengl shader
	OglsShaderCreateInfo shaderCreateInfo{};
	shaderCreateInfo.vertexSrc = vertexShaderSource;
	shaderCreateInfo.fragmentSrc = fragmentShaderSource;

	OglsShader* shader;
	ogls::createShaderFromStr(&shader, &shaderCreateInfo);


	// setup batch groups
	BatchGroup batch{};
	batch.vertexBuffer = vertexBuffer;
	batch.indexBuffer = indexBuffer;
	batch.vertexArray = vertexArray;


	std::vector<std::vector<Cell>> spaces(CELL_SPACE_WIDTH);
	std::vector<std::vector<bool>> aliveSpaces(CELL_SPACE_WIDTH);

	for (uint32_t i = 0; i < spaces.size(); i++)
	{
		spaces[i].resize(CELL_SPACE_HEIGHT);
		aliveSpaces[i].resize(CELL_SPACE_HEIGHT);

		for (uint32_t j = 0; j < spaces[i].size(); j++)
		{
			spaces[i][j].x = i;
			spaces[i][j].y = j;
			spaces[i][j].alive = false;
			aliveSpaces[i][j] = false;
		}
	}

	int x = CELL_SPACE_WIDTH / 2;
	int y = CELL_SPACE_HEIGHT / 2;

	spaces[x + 0][y + 0].alive = true;
	spaces[x + 0][y + 1].alive = true;
	spaces[x - 1][y + 0].alive = true;
	spaces[x + 0][y - 1].alive = true;
	spaces[x + 1][y - 1].alive = true;

	aliveSpaces[x + 0][y + 0] = true;
	aliveSpaces[x + 0][y + 1] = true;
	aliveSpaces[x - 1][y + 0] = true;
	aliveSpaces[x + 0][y - 1] = true;
	aliveSpaces[x + 1][y - 1] = true;

	float camx = CELL_SPACE_WIDTH * CELL_SPACE_SCALE * 0.5f, camy = CELL_SPACE_HEIGHT * CELL_SPACE_SCALE * 0.5f;
	float scale = 1.0f;
	bool p_open = false, pressOnce = false, follow = false;
	uint32_t generation = 0;

	bool pause = false, iterate = false;
	std::string pauseName = "pause";

	int editx = CELL_SPACE_WIDTH / 2, edity = CELL_SPACE_HEIGHT / 2;

	Timer timer{};
	timer.start();

	float oldTime = 0.0f;
	Timer deltaTime{};
	deltaTime.start();

	Timer timeStep{};
	timeStep.start();
	float timeInt = 0.2f;

	glViewport(0, 0, 1280, 800);

    srand((unsigned)time(0));
	int distribution = 2;
	int concentration = 33;
	int concRadius = 6;


	printf("Conway's game of life simulation in OpenGL and C++\n");
	printf("Note: Press the \'c\' key to open the settings\n");

    while (!glfwWindowShouldClose(window))
    {
		float timeNow = deltaTime.elapsed();
		float dt = timeNow - oldTime;
		oldTime = timeNow;


		// begin render
		glClearColor(COLOR_BG, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT);

		int width, height;
		glfwGetWindowSize(window, &width, &height);

		cameraMovement(window, &camx, &camy, dt);
		cameraScale(window, &scale, dt);

		if (camx > CELL_SPACE_WIDTH * CELL_SPACE_SCALE) camx = CELL_SPACE_WIDTH * CELL_SPACE_SCALE;
		if (camx < 0) camx = 0;
		if (camy > CELL_SPACE_HEIGHT * CELL_SPACE_SCALE) camy = CELL_SPACE_HEIGHT * CELL_SPACE_SCALE;
		if (camy < 0) camy = 0;

		glm::mat4 proj = glm::ortho(-static_cast<float>(width) * 0.5f * scale, static_cast<float>(width) * 0.5f * scale, -static_cast<float>(height) * 0.5f * scale, static_cast<float>(height) * 0.5f * scale);
		glm::mat4 view = glm::inverse(glm::translate(glm::mat4(1.0f), glm::vec3(camx, camy, 0.0f)));
		glm::mat4 camera = proj * view;

		ogls::bindShader(shader);
		glUniformMatrix4fv(glGetUniformLocation(ogls::getShaderId(shader), "u_Camera"), 1, GL_FALSE, glm::value_ptr(camera));


		clearDrawList(&batch);

		bool calculate = false;
		if (timeStep.elapsed() >= timeInt)
		{
			calculate = true;
			timeStep.reset();
		}

		if ((!pause && calculate) || iterate)
			generation++;


		// calculate and draw cells
		for (uint32_t i = 1; i < spaces.size() - 1; i++)
		{
			for (uint32_t j = 1; j < spaces[i].size() - 1; j++)
			{
				Cell& cell = spaces[i][j];

				if ((!pause && calculate) || iterate)
				{
					// calculate cell generation
					int neighbors = 0;
					if (aliveSpaces[i-1][j]) neighbors++; // left
					if (aliveSpaces[i+1][j]) neighbors++; // right
					if (aliveSpaces[i][j-1]) neighbors++; // bottom
					if (aliveSpaces[i][j+1]) neighbors++; // top
					if (aliveSpaces[i-1][j-1]) neighbors++; // botom left
					if (aliveSpaces[i-1][j+1]) neighbors++; // top left
					if (aliveSpaces[i+1][j-1]) neighbors++; // bottom right
					if (aliveSpaces[i+1][j+1]) neighbors++; // top right

					if (cell.alive && neighbors < 2) cell.alive = false;
					else if (cell.alive && neighbors > 3) cell.alive = false;
					else if (cell.alive && (neighbors == 2 || neighbors == 3)) cell.alive = true;
					else if (!cell.alive && neighbors == 3) cell.alive = true;
				}

				// draw cells
				if (cell.alive)
					drawRect(&batch, {cell.x * CELL_SPACE_SCALE, cell.y * CELL_SPACE_SCALE}, {10.0f, 10.0f}, {COLOR_FG});
				else
					drawRect(&batch, {cell.x * CELL_SPACE_SCALE, cell.y * CELL_SPACE_SCALE}, {10.0f, 10.0f}, {COLOR_FG2});
			}
		}

		for (uint32_t i = 1; i < spaces.size() - 1; i++)
		{
			for (uint32_t j = 1; j < spaces[i].size() - 1; j++)
			{
				aliveSpaces[i][j] = spaces[i][j].alive;
			}
		}

		// check for cells on border
		for (uint32_t i = 0; i < spaces.size(); i++)
		{
			Cell& cell = spaces[i][0];
			cell.alive = false;
			drawRect(&batch, {cell.x * CELL_SPACE_SCALE + 3.0f, cell.y * CELL_SPACE_SCALE + 3.0f}, {4.0f, 4.0f}, {COLOR_RED});
			Cell cell2 = spaces[i].back();
			cell2.alive = false;
			drawRect(&batch, {cell2.x * CELL_SPACE_SCALE + 3.0f, cell2.y * CELL_SPACE_SCALE + 3.0f}, {4.0f, 4.0f}, {COLOR_RED});
		}
		for (uint32_t i = 0; i < spaces[0].size(); i++)
		{
			Cell& cell = spaces[0][i];
			cell.alive = false;
			drawRect(&batch, {cell.x * CELL_SPACE_SCALE + 3.0f, cell.y * CELL_SPACE_SCALE + 3.0f}, {4.0f, 4.0f}, {COLOR_RED});
			Cell cell2 = spaces.back()[i];
			cell2.alive = false;
			drawRect(&batch, {cell2.x * CELL_SPACE_SCALE + 3.0f, cell2.y * CELL_SPACE_SCALE + 3.0f}, {4.0f, 4.0f}, {COLOR_RED});
		}

		// draw the cells
		submitDrawList(&batch);

		// imgui
		int key = glfwGetKey(window, GLFW_KEY_C) == GLFW_PRESS;
		if (key == GLFW_PRESS && !pressOnce)
		{
			if (!p_open)
				p_open = true;
			else
				p_open = false;
			
			pressOnce = true;
		}
		else if (key == GLFW_RELEASE)
			pressOnce = false;

		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();

		if (p_open)
		{
			ImGui::Begin("Settings", &p_open);
			ImGui::Text("Conway's game of life simulation in OpenGL and C++");
			ImGui::Text("- Use (wasd) to move the camera around");
			ImGui::Text("- Press (-) and (+) to zoom in and out");
			ImGui::Text("- Hold down shift to increase speed and zoom");
			ImGui::Text("- Cells will die when on the border (squares marked red)");
			ImGui::NewLine();

			if (ImGui::Button(pauseName.c_str()))
			{
				if (pause)
				{
					pause = false;
					pauseName = "Pause";
					timer.play();
				}
				else
				{
					pause = true;
					pauseName = "Play";
					timer.pause();
				}
			}
			ImGui::SameLine();
			iterate = false;
			if (ImGui::Button("Iterate"))
			{
				iterate = true;
			}
			ImGui::SameLine();
			if (ImGui::Button("Clear"))
			{
				for (uint32_t i = 0; i < spaces.size(); i++)
				{
					for (uint32_t j = 0; j < spaces[i].size(); j++)
					{
						spaces[i][j].alive = false;
						aliveSpaces[i][j] = false;
					}
				}
			}

			ImGui::Spacing();

			if (ImGui::CollapsingHeader("Editor"))
			{
				ImGui::Text("add or remove cells with the red cursor");
				ImGui::Text("use the button pads or use the (hjkl) keys to move the cursor");
				ImGui::Text("press the space key to add/remove cell");
				ImGui::TextColored(ImVec4(COLOR_RED,1), "Note: pause the game to prevent cells from immediately dying");
				ImGui::Spacing();

				if (ImGui::IsKeyPressed(ImGuiKey_K))
					edity++;
				if (ImGui::IsKeyPressed(ImGuiKey_H))
					editx--;
				if (ImGui::IsKeyPressed(ImGuiKey_L))
					editx++;
				if (ImGui::IsKeyPressed(ImGuiKey_J))
					edity--;

				if (ImGui::IsKeyPressed(ImGuiKey_Space))
				{
					spaces[editx][edity].alive = !spaces[editx][edity].alive;
					aliveSpaces[editx][edity] = !aliveSpaces[editx][edity];
				}

				ImGui::Indent(20);
				if (ImGui::Button("^", ImVec2(20, 20)))
					edity++;
				ImGui::Unindent(20);
				if (ImGui::Button("<", ImVec2(20, 20)))
					editx--;
				ImGui::SameLine(0, 20);
				if (ImGui::Button(">", ImVec2(20, 20)))
					editx++;
				ImGui::Indent(20);
				if (ImGui::Button("v", ImVec2(20, 20)))
					edity--;
				ImGui::Unindent(20);

				ImGui::Spacing();

				if (editx > CELL_SPACE_WIDTH - 1) editx = CELL_SPACE_WIDTH - 1;
				if (editx < 0) editx = 0;
				if (edity > CELL_SPACE_HEIGHT - 1) edity = CELL_SPACE_HEIGHT - 1;
				if (edity < 0) edity = 0;

				if (ImGui::Button("Place Cell"))
				{
					spaces[editx][edity].alive = true;
					aliveSpaces[editx][edity] = true;
				}
				ImGui::SameLine();
				if (ImGui::Button("Remove Cell"))
				{
					spaces[editx][edity].alive = false;
					aliveSpaces[editx][edity] = false;
				}

				ImGui::Spacing();
				if (ImGui::Button("Go to cursor"))
				{
					camx = editx * CELL_SPACE_SCALE;
					camy = edity * CELL_SPACE_SCALE;
				}
				ImGui::SameLine();
				if (ImGui::Button("Cursor to center"))
				{
					editx = CELL_SPACE_WIDTH / 2;
					edity = CELL_SPACE_HEIGHT / 2;
				}

				ImGui::Checkbox("Follow cursor", &follow);
				if (follow)
				{
					camx = editx * CELL_SPACE_SCALE;
					camy = edity * CELL_SPACE_SCALE;
				}

				ImGui::NewLine();
				if (ImGui::Button("Fill all cells"))
				{
					for (uint32_t i = 0; i < spaces.size(); i++)
					{
						for (uint32_t j = 0; j < spaces[i].size(); j++)
						{
							spaces[i][j].alive = true;
							aliveSpaces[i][j] = true;
						}
					}
				}
				ImGui::SameLine();
				if (ImGui::Button("Remove all cells"))
				{
					for (uint32_t i = 0; i < spaces.size(); i++)
					{
						for (uint32_t j = 0; j < spaces[i].size(); j++)
						{
							spaces[i][j].alive = false;
							aliveSpaces[i][j] = false;
						}
					}
				}

				ImGui::NewLine();
				if (ImGui::Button("Fill Randomly"))
				{
					for (int i = 0; i < spaces.size(); i++)
					{
						for (int j = 0; j < spaces[i].size(); j++)
						{
							bool alive = (rand() % distribution) == 0 ? true : false;

							spaces[i][j].alive = alive;
							aliveSpaces[i][j] = alive;

							if (alive)
							{
								for (int k = i - concRadius; k < i + concRadius; k++)
								{
									for (int l = j - concRadius; l < j + concRadius; l++)
									{
										int left = k;
										int bottom = l;

										if (left < 0) left = 0;
										if (left > CELL_SPACE_WIDTH - 1) left = CELL_SPACE_WIDTH - 1;
										if (bottom > CELL_SPACE_HEIGHT - 1) bottom = CELL_SPACE_HEIGHT - 1;
										if (bottom < 0) bottom = 0;

										bool neighborAlive = (rand() % 100 + 1) <= concentration ? true : false;
										spaces[left][bottom].alive = neighborAlive;
										aliveSpaces[left][bottom] = neighborAlive;
									}
								}
							}
						}
					}
				}
				ImGui::SliderInt("Distribution", &distribution, 1, 100);
				ImGui::SliderInt("Concentration", &concentration, 1, 100);
				ImGui::SliderInt("Concentration Radius", &concRadius, 1, 50);

				ImGui::NewLine();

				if (ImGui::Button("Print Pattern to terminal"))
				{
					printf("Pattern Coords:\n");
					for (int i = 0; i < spaces.size(); i++)
					{
						for (int j = 0; j < spaces[i].size(); j++)
						{
							if (aliveSpaces[i][j])
							{
								printf("[%d][%d]\n", i, j);
							}
						}
					}
				}
				ImGui::NewLine();

				drawRectImmediate(&batch, {editx * CELL_SPACE_SCALE + 3.0f, edity * CELL_SPACE_SCALE + 3.0f}, {4.0f, 4.0f}, {COLOR_RED});
			}

			if (ImGui::CollapsingHeader("Presets"))
			{
				ImGui::Text("Choose a pattern");
				if (ImGui::Button("Beacon"))
				{
					for (int i = 0; i < spaces.size(); i++)
					{
						for (int j = 0; j < spaces[i].size(); j++)
						{
							spaces[i][j].alive = false;
							aliveSpaces[i][j] = false;
						}
					}

					spaces[x + 0][y + 0].alive = true; aliveSpaces[x + 0][y + 0] = true;
					spaces[x + 1][y + 0].alive = true; aliveSpaces[x + 1][y + 0] = true;
					spaces[x + 0][y - 1].alive = true; aliveSpaces[x + 0][y - 1] = true;
					spaces[x + 3][y - 2].alive = true; aliveSpaces[x + 3][y - 2] = true;
					spaces[x + 3][y - 3].alive = true; aliveSpaces[x + 3][y - 3] = true;
					spaces[x + 2][y - 3].alive = true; aliveSpaces[x + 2][y - 3] = true;
				}
				if (ImGui::Button("Glider"))
				{
					for (int i = 0; i < spaces.size(); i++)
					{
						for (int j = 0; j < spaces[i].size(); j++)
						{
							spaces[i][j].alive = false;
							aliveSpaces[i][j] = false;
						}
					}

					spaces[x + 0][y + 0].alive = true;
					spaces[x + 1][y + 0].alive = true;
					spaces[x + 2][y + 0].alive = true;
					spaces[x + 1][y + 2].alive = true;
					spaces[x + 2][y + 1].alive = true;

					aliveSpaces[x + 0][y + 0] = true;
					aliveSpaces[x + 1][y + 0] = true;
					aliveSpaces[x + 2][y + 0] = true;
					aliveSpaces[x + 1][y + 2] = true;
					aliveSpaces[x + 2][y + 1] = true;
				}
				if (ImGui::Button("Gosper glider gun"))
				{
					for (int i = 0; i < spaces.size(); i++)
					{
						for (int j = 0; j < spaces[i].size(); j++)
						{
							spaces[i][j].alive = false;
							aliveSpaces[i][j] = false;
						}
					}

					spaces[x - 1][y - 1].alive = true;  aliveSpaces[x - 1][y - 1] = true;
					spaces[x - 2][y + 0].alive = true;  aliveSpaces[x - 2][y + 0] = true;
					spaces[x - 2][y - 1].alive = true;  aliveSpaces[x - 2][y - 1] = true;
					spaces[x - 2][y - 2].alive = true;  aliveSpaces[x - 2][y - 2] = true;
					spaces[x - 3][y + 1].alive = true;  aliveSpaces[x - 3][y + 1] = true;
					spaces[x - 3][y - 3].alive = true;  aliveSpaces[x - 3][y - 3] = true;
					spaces[x - 4][y - 1].alive = true;  aliveSpaces[x - 4][y - 1] = true;
					spaces[x - 5][y + 2].alive = true;  aliveSpaces[x - 5][y + 2] = true;
					spaces[x - 5][y - 4].alive = true;  aliveSpaces[x - 5][y - 4] = true;
					spaces[x - 6][y + 2].alive = true;  aliveSpaces[x - 6][y + 2] = true;
					spaces[x - 6][y - 4].alive = true;  aliveSpaces[x - 6][y - 4] = true;
					spaces[x - 7][y + 1].alive = true;  aliveSpaces[x - 7][y + 1] = true;
					spaces[x - 7][y - 3].alive = true;  aliveSpaces[x - 7][y - 3] = true;
					spaces[x - 8][y + 0].alive = true;  aliveSpaces[x - 8][y + 0] = true;
					spaces[x - 8][y - 1].alive = true;  aliveSpaces[x - 8][y - 1] = true;
					spaces[x - 8][y - 2].alive = true;  aliveSpaces[x - 8][y - 2] = true;
					spaces[x - 17][y + 0].alive = true; aliveSpaces[x - 17][y + 0] = true;
					spaces[x - 17][y - 1].alive = true; aliveSpaces[x - 17][y - 1] = true;
					spaces[x - 18][y + 0].alive = true; aliveSpaces[x - 18][y + 0] = true;
					spaces[x - 18][y - 1].alive = true; aliveSpaces[x - 18][y - 1] = true;
					spaces[x + 2][y + 0].alive = true;  aliveSpaces[x + 2][y + 0] = true;
					spaces[x + 2][y + 1].alive = true;  aliveSpaces[x + 2][y + 1] = true;
					spaces[x + 2][y + 2].alive = true;  aliveSpaces[x + 2][y + 2] = true;
					spaces[x + 3][y + 0].alive = true;  aliveSpaces[x + 3][y + 0] = true;
					spaces[x + 3][y + 1].alive = true;  aliveSpaces[x + 3][y + 1] = true;
					spaces[x + 3][y + 2].alive = true;  aliveSpaces[x + 3][y + 2] = true;
					spaces[x + 4][y - 1].alive = true;  aliveSpaces[x + 4][y - 1] = true;
					spaces[x + 4][y + 3].alive = true;  aliveSpaces[x + 4][y + 3] = true;
					spaces[x + 6][y + 3].alive = true;  aliveSpaces[x + 6][y + 3] = true;
					spaces[x + 6][y + 4].alive = true;  aliveSpaces[x + 6][y + 4] = true;
					spaces[x + 6][y - 1].alive = true;  aliveSpaces[x + 6][y - 1] = true;
					spaces[x + 6][y - 2].alive = true;  aliveSpaces[x + 6][y - 2] = true;
					spaces[x + 16][y + 1].alive = true; aliveSpaces[x + 16][y + 1] = true;
					spaces[x + 16][y + 2].alive = true; aliveSpaces[x + 16][y + 2] = true;
					spaces[x + 17][y + 1].alive = true; aliveSpaces[x + 17][y + 1] = true;
					spaces[x + 17][y + 2].alive = true; aliveSpaces[x + 17][y + 2] = true;
				}
				if (ImGui::Button("R-pentomino"))
				{
					for (int i = 0; i < spaces.size(); i++)
					{
						for (int j = 0; j < spaces[i].size(); j++)
						{
							spaces[i][j].alive = false;
							aliveSpaces[i][j] = false;
						}
					}

					spaces[x + 0][y + 0].alive = true;
					spaces[x + 0][y + 1].alive = true;
					spaces[x - 1][y + 0].alive = true;
					spaces[x + 0][y - 1].alive = true;
					spaces[x + 1][y - 1].alive = true;

					aliveSpaces[x + 0][y + 0] = true;
					aliveSpaces[x + 0][y + 1] = true;
					aliveSpaces[x - 1][y + 0] = true;
					aliveSpaces[x + 0][y - 1] = true;
					aliveSpaces[x + 1][y - 1] = true;
				}
				if (ImGui::Button("Penta-decathlon"))
				{
					for (int i = 0; i < spaces.size(); i++)
					{
						for (int j = 0; j < spaces[i].size(); j++)
						{
							spaces[i][j].alive = false;
							aliveSpaces[i][j] = false;
						}
					}

					spaces[x + 0][y + 0].alive = true; aliveSpaces[x + 0][y + 0] = true;
					spaces[x - 1][y + 0].alive = true; aliveSpaces[x - 1][y + 0] = true;
					spaces[x - 2][y + 1].alive = true; aliveSpaces[x - 2][y + 1] = true;
					spaces[x - 2][y - 1].alive = true; aliveSpaces[x - 2][y - 1] = true;
					spaces[x - 3][y + 0].alive = true; aliveSpaces[x - 3][y + 0] = true;
					spaces[x - 4][y + 0].alive = true; aliveSpaces[x - 4][y + 0] = true;
					spaces[x + 1][y + 0].alive = true; aliveSpaces[x + 1][y + 0] = true;
					spaces[x + 2][y + 0].alive = true; aliveSpaces[x + 2][y + 0] = true;
					spaces[x + 3][y + 1].alive = true; aliveSpaces[x + 3][y + 1] = true;
					spaces[x + 3][y - 1].alive = true; aliveSpaces[x + 3][y - 1] = true;
					spaces[x + 4][y + 0].alive = true; aliveSpaces[x + 4][y + 0] = true;
					spaces[x + 5][y + 0].alive = true; aliveSpaces[x + 5][y + 0] = true;
				}
			}

			ImGui::NewLine();
			ImGui::Text("Conway's game of life rules:");
			ImGui::Text("1. Any live cell with fewer than two live neighbours dies, as if by underpopulation");
			ImGui::Text("2. Any live cell with two or three live neighbours lives on to the next generation");
			ImGui::Text("3. Any live cell with more than three live neighbours dies, as if by overpopulation");
			ImGui::Text("4. Any dead cell with exactly three live neighbours becomes a live cell, as if by reproduction");
			ImGui::NewLine();
			ImGui::Text("Time elapsed: %f", timer.elapsed());
			ImGui::Text("Generation: %u", generation);
			ImGui::SliderFloat("Time step", &timeInt, 0.01f, 1.0f);

			ImGui::NewLine();
			if (ImGui::Button("Reset"))
			{
				timer.reset();
				generation = 0;
				for (int i = 0; i < spaces.size(); i++)
				{
					for (int j = 0; j < spaces[i].size(); j++)
					{
						spaces[i][j].alive = false;
						aliveSpaces[i][j] = false;
					}
				}

				spaces[x + 0][y + 0].alive = true;
				spaces[x + 0][y + 1].alive = true;
				spaces[x - 1][y + 0].alive = true;
				spaces[x + 0][y - 1].alive = true;
				spaces[x + 1][y - 1].alive = true;

				aliveSpaces[x + 0][y + 0] = true;
				aliveSpaces[x + 0][y + 1] = true;
				aliveSpaces[x - 1][y + 0] = true;
				aliveSpaces[x + 0][y - 1] = true;
				aliveSpaces[x + 1][y - 1] = true;
			}

			ImGui::End();
		}

		ImGui::Render();
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());



		glfwSwapBuffers(window);
        glfwPollEvents();
    }


	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();


	ogls::destroyShader(shader);
	ogls::destroyVertexArray(vertexArray);
	ogls::destroyIndexBuffer(indexBuffer);
	ogls::destroyVertexBuffer(vertexBuffer);


    glfwTerminate();
    return 0;
}
