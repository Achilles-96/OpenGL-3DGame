#include <iostream>
#include <cmath>
#include <fstream>
#include <vector>
#include <algorithm>
#include <string>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <FTGL/ftgl.h>
#include <SOIL/SOIL.h>

#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <ao/ao.h>
#include <mpg123.h>
#include <unistd.h>
#include <signal.h>

#define BITS 8

pid_t pid;

using namespace std;
void reshapeWindow (GLFWwindow* window, int width, int height);

class VAO {
	public:
		GLuint VertexArrayID;
		GLuint VertexBuffer;
		GLuint ColorBuffer;
		GLuint TextureBuffer;
		GLuint TextureID;

		GLenum PrimitiveMode;
		GLenum FillMode;
		int NumVertices;

		VAO(){
		}
};
//typedef struct VAO VAO;

struct GLMatrices {
	glm::mat4 projection;
	glm::mat4 model;
	glm::mat4 view;
	GLuint MatrixID;
	GLuint TexMatrixID; // For use with texture shader
} Matrices;

struct FTGLFont {
	FTFont* font;
	GLuint fontMatrixID;
	GLuint fontColorID;
} GL3Font;

GLuint programID, fontProgramID, textureProgramID;;

/* Function to load Shaders - Use it as it is */
GLuint LoadShaders(const char * vertex_file_path,const char * fragment_file_path) {

	// Create the shaders
	GLuint VertexShaderID = glCreateShader(GL_VERTEX_SHADER);
	GLuint FragmentShaderID = glCreateShader(GL_FRAGMENT_SHADER);

	// Read the Vertex Shader code from the file
	std::string VertexShaderCode;
	std::ifstream VertexShaderStream(vertex_file_path, std::ios::in);
	if(VertexShaderStream.is_open())
	{
		std::string Line = "";
		while(getline(VertexShaderStream, Line))
			VertexShaderCode += "\n" + Line;
		VertexShaderStream.close();
	}

	// Read the Fragment Shader code from the file
	std::string FragmentShaderCode;
	std::ifstream FragmentShaderStream(fragment_file_path, std::ios::in);
	if(FragmentShaderStream.is_open()){
		std::string Line = "";
		while(getline(FragmentShaderStream, Line))
			FragmentShaderCode += "\n" + Line;
		FragmentShaderStream.close();
	}

	GLint Result = GL_FALSE;
	int InfoLogLength;

	// Compile Vertex Shader
	printf("Compiling shader : %s\n", vertex_file_path);
	char const * VertexSourcePointer = VertexShaderCode.c_str();
	glShaderSource(VertexShaderID, 1, &VertexSourcePointer , NULL);
	glCompileShader(VertexShaderID);

	// Check Vertex Shader
	glGetShaderiv(VertexShaderID, GL_COMPILE_STATUS, &Result);
	glGetShaderiv(VertexShaderID, GL_INFO_LOG_LENGTH, &InfoLogLength);
	std::vector<char> VertexShaderErrorMessage(InfoLogLength);
	glGetShaderInfoLog(VertexShaderID, InfoLogLength, NULL, &VertexShaderErrorMessage[0]);
	fprintf(stdout, "%s\n", &VertexShaderErrorMessage[0]);

	// Compile Fragment Shader
	printf("Compiling shader : %s\n", fragment_file_path);
	char const * FragmentSourcePointer = FragmentShaderCode.c_str();
	glShaderSource(FragmentShaderID, 1, &FragmentSourcePointer , NULL);
	glCompileShader(FragmentShaderID);

	// Check Fragment Shader
	glGetShaderiv(FragmentShaderID, GL_COMPILE_STATUS, &Result);
	glGetShaderiv(FragmentShaderID, GL_INFO_LOG_LENGTH, &InfoLogLength);
	std::vector<char> FragmentShaderErrorMessage(InfoLogLength);
	glGetShaderInfoLog(FragmentShaderID, InfoLogLength, NULL, &FragmentShaderErrorMessage[0]);
	fprintf(stdout, "%s\n", &FragmentShaderErrorMessage[0]);

	// Link the program
	fprintf(stdout, "Linking program\n");
	GLuint ProgramID = glCreateProgram();
	glAttachShader(ProgramID, VertexShaderID);
	glAttachShader(ProgramID, FragmentShaderID);
	glLinkProgram(ProgramID);

	// Check the program
	glGetProgramiv(ProgramID, GL_LINK_STATUS, &Result);
	glGetProgramiv(ProgramID, GL_INFO_LOG_LENGTH, &InfoLogLength);
	std::vector<char> ProgramErrorMessage( max(InfoLogLength, int(1)) );
	glGetProgramInfoLog(ProgramID, InfoLogLength, NULL, &ProgramErrorMessage[0]);
	fprintf(stdout, "%s\n", &ProgramErrorMessage[0]);

	glDeleteShader(VertexShaderID);
	glDeleteShader(FragmentShaderID);

	return ProgramID;
}

static void error_callback(int error, const char* description)
{
	fprintf(stderr, "Error: %s\n", description);
}

void quit(GLFWwindow *window)
{
	glfwDestroyWindow(window);
	glfwTerminate();
	kill(pid,SIGKILL);
	exit(EXIT_SUCCESS);
}

glm::vec3 getRGBfromHue (int hue)
{
	float intp;
	float fracp = modff(hue/60.0, &intp);
	float x = 1.0 - abs((float)((int)intp%2)+fracp-1.0);

	if (hue < 60)
		return glm::vec3(1,x,0);
	else if (hue < 120)
		return glm::vec3(x,1,0);
	else if (hue < 180)
		return glm::vec3(0,1,x);
	else if (hue < 240)
		return glm::vec3(0,x,1);
	else if (hue < 300)
		return glm::vec3(x,0,1);
	else
		return glm::vec3(1,0,x);
}


/* Generate VAO, VBOs and return VAO handle */
VAO* create3DObject (GLenum primitive_mode, int numVertices, const GLfloat* vertex_buffer_data, const GLfloat* color_buffer_data, GLenum fill_mode )
{
	VAO* vao = new  VAO();
	vao->PrimitiveMode = primitive_mode;
	vao->NumVertices = numVertices;
	vao->FillMode = fill_mode;

	// Create Vertex Array Object
	// Should be done after CreateWindow and before any other GL calls
	glGenVertexArrays(1, &(vao->VertexArrayID)); // VAO
	glGenBuffers (1, &(vao->VertexBuffer)); // VBO - vertices
	glGenBuffers (1, &(vao->ColorBuffer));  // VBO - colors

	glBindVertexArray (vao->VertexArrayID); // Bind the VAO 
	glBindBuffer (GL_ARRAY_BUFFER, vao->VertexBuffer); // Bind the VBO vertices 
	glBufferData (GL_ARRAY_BUFFER, 3*numVertices*sizeof(GLfloat), vertex_buffer_data, GL_STATIC_DRAW); // Copy the vertices into VBO
	glVertexAttribPointer(
			0,                  // attribute 0. Vertices
			3,                  // size (x,y,z)
			GL_FLOAT,           // type
			GL_FALSE,           // normalized?
			0,                  // stride
			(void*)0            // array buffer offset
			);

	glBindBuffer (GL_ARRAY_BUFFER, vao->ColorBuffer); // Bind the VBO colors 
	glBufferData (GL_ARRAY_BUFFER, 3*numVertices*sizeof(GLfloat), color_buffer_data, GL_STATIC_DRAW);  // Copy the vertex colors
	glVertexAttribPointer(
			1,                  // attribute 1. Color
			3,                  // size (r,g,b)
			GL_FLOAT,           // type
			GL_FALSE,           // normalized?
			0,                  // stride
			(void*)0            // array buffer offset
			);

	return vao;
}

/* Generate VAO, VBOs and return VAO handle - Common Color for all vertices */
VAO* create3DObject (GLenum primitive_mode, int numVertices, const GLfloat* vertex_buffer_data, const GLfloat red, const GLfloat green, const GLfloat blue,  GLenum fill_mode)
{
	GLfloat* color_buffer_data = new GLfloat [3*numVertices];
	for (int i=0; i<numVertices; i++) {
		color_buffer_data [3*i] = red;
		color_buffer_data [3*i + 1] = green;
		color_buffer_data [3*i + 2] = blue;
	}

	return create3DObject(primitive_mode, numVertices, vertex_buffer_data, color_buffer_data, fill_mode);
}


struct VAO* create3DTexturedObject (GLenum primitive_mode, int numVertices, const GLfloat* vertex_buffer_data, const GLfloat* texture_buffer_data, GLuint textureID, GLenum fill_mode=GL_FILL)
{
	struct VAO* vao = new struct VAO;
	vao->PrimitiveMode = primitive_mode;
	vao->NumVertices = numVertices;
	vao->FillMode = fill_mode;
	vao->TextureID = textureID;

	// Create Vertex Array Object
	// Should be done after CreateWindow and before any other GL calls
	glGenVertexArrays(1, &(vao->VertexArrayID)); // VAO
	glGenBuffers (1, &(vao->VertexBuffer)); // VBO - vertices
	glGenBuffers (1, &(vao->TextureBuffer));  // VBO - textures

	glBindVertexArray (vao->VertexArrayID); // Bind the VAO
	glBindBuffer (GL_ARRAY_BUFFER, vao->VertexBuffer); // Bind the VBO vertices
	glBufferData (GL_ARRAY_BUFFER, 3*numVertices*sizeof(GLfloat), vertex_buffer_data, GL_STATIC_DRAW); // Copy the vertices into VBO
	glVertexAttribPointer(
			0,                  // attribute 0. Vertices
			3,                  // size (x,y,z)
			GL_FLOAT,           // type
			GL_FALSE,           // normalized?
			0,                  // stride
			(void*)0            // array buffer offset
			);

	glBindBuffer (GL_ARRAY_BUFFER, vao->TextureBuffer); // Bind the VBO textures
	glBufferData (GL_ARRAY_BUFFER, 2*numVertices*sizeof(GLfloat), texture_buffer_data, GL_STATIC_DRAW);  // Copy the vertex colors
	glVertexAttribPointer(
			2,                  // attribute 2. Textures
			2,                  // size (s,t)
			GL_FLOAT,           // type
			GL_FALSE,           // normalized?
			0,                  // stride
			(void*)0            // array buffer offset
			);

	return vao;
}

/* Render the VBOs handled by VAO */
void draw3DObject (VAO* vao)
{
	// Change the Fill Mode for this object
	glPolygonMode (GL_FRONT_AND_BACK, vao->FillMode);

	// Bind the VAO to use
	glBindVertexArray (vao->VertexArrayID);

	// Enable Vertex Attribute 0 - 3d Vertices
	glEnableVertexAttribArray(0);
	// Bind the VBO to use
	glBindBuffer(GL_ARRAY_BUFFER, vao->VertexBuffer);

	// Enable Vertex Attribute 1 - Color
	glEnableVertexAttribArray(1);
	// Bind the VBO to use
	glBindBuffer(GL_ARRAY_BUFFER, vao->ColorBuffer);

	// Draw the geometry !
	glDrawArrays(vao->PrimitiveMode, 0, vao->NumVertices); // Starting from vertex 0; 3 vertices total -> 1 triangle
}

void draw3DTexturedObject (struct VAO* vao)
{
	// Change the Fill Mode for this object
	glPolygonMode (GL_FRONT_AND_BACK, vao->FillMode);

	// Bind the VAO to use
	glBindVertexArray (vao->VertexArrayID);

	// Enable Vertex Attribute 0 - 3d Vertices
	glEnableVertexAttribArray(0);
	// Bind the VBO to use
	glBindBuffer(GL_ARRAY_BUFFER, vao->VertexBuffer);

	// Bind Textures using texture units
	glBindTexture(GL_TEXTURE_2D, vao->TextureID);

	// Enable Vertex Attribute 2 - Texture
	glEnableVertexAttribArray(2);
	// Bind the VBO to use
	glBindBuffer(GL_ARRAY_BUFFER, vao->TextureBuffer);

	// Draw the geometry !
	glDrawArrays(vao->PrimitiveMode, 0, vao->NumVertices); // Starting from vertex 0; 3 vertices total -> 1 triangle

	// Unbind Textures to be safe
	glBindTexture(GL_TEXTURE_2D, 0);
}

/* Create an OpenGL Texture from an image */
GLuint createTexture (const char* filename)
{
	GLuint TextureID;
	// Generate Texture Buffer
	glGenTextures(1, &TextureID);
	// All upcoming GL_TEXTURE_2D operations now have effect on our texture buffer
	glBindTexture(GL_TEXTURE_2D, TextureID);
	// Set our texture parameters
	// Set texture wrapping to GL_REPEAT
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	// Set texture filtering (interpolation)
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	// Load image and create OpenGL texture
	int twidth, theight;
	unsigned char* image = SOIL_load_image(filename, &twidth, &theight, 0, SOIL_LOAD_RGB);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, twidth, theight, 0, GL_RGB, GL_UNSIGNED_BYTE, image);
	glGenerateMipmap(GL_TEXTURE_2D); // Generate MipMaps to use
	SOIL_free_image_data(image); // Free the data read from file after creating opengl texture
	glBindTexture(GL_TEXTURE_2D, 0); // Unbind texture when done, so we won't accidentily mess it up

	return TextureID;
}

/**************************
 * Customizable functions *
 **************************/

float screenleft = -600.0f, screenright = 600.0f, screentop = -300.0f, screenbotton = 300.0f, screennear = -500.0f, screenfar = 500.0f;
double curx, cury;
/* Executed when a regular key is pressed/released/held-down */
/* Prefered for Keyboard events */
void keyboard (GLFWwindow* window, int key, int scancode, int action, int mods)
{
	// Function is called first on GLFW_PRESS.

	if (action == GLFW_RELEASE) {
		switch (key) {
			case GLFW_KEY_C:
				//rectangle_rot_status = !rectangle_rot_status;
				break;
			case GLFW_KEY_KP_ADD:
				//triangle_rot_status = !triangle_rot_status;
				break;
			case GLFW_KEY_KP_SUBTRACT:
				break;
			case GLFW_KEY_LEFT:
				//				panleft = 0;
				break;
			case GLFW_KEY_RIGHT:
				//				panright = 0;
				break;
			case GLFW_KEY_UP:
				//				panup = 0;
				break;
			case GLFW_KEY_DOWN:
				//				pandown = 0;
				break;
			case GLFW_KEY_X:
				// do something ..
				break;
			default:
				break;
		}
	}
	else if (action == GLFW_PRESS) {
		switch (key) {
			case GLFW_KEY_ESCAPE:
				quit(window);
				break;
			case GLFW_KEY_KP_ADD:
				//				zoominstate=1;
				break;
			case GLFW_KEY_KP_SUBTRACT:
				//				zoomoutstate = 1;
				break;
			case GLFW_KEY_LEFT:
				//				panleft = 1;
				break;
			case GLFW_KEY_RIGHT:
				//				panright = 1;
				break;
			case GLFW_KEY_UP:
				//				panup = 1;
				break;
			case GLFW_KEY_DOWN:
				//				pandown = 1;
				break;
			default:
				break;
		}
	}
}

/* Executed for character input (like in text boxes) */
void keyboardChar (GLFWwindow* window, unsigned int key)
{
	switch (key) {
		case 'Q':
		case 'q':
			quit(window);
			break;
		default:
			break;
	}
}

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
	if(yoffset == 1){
		/*		screenleft /= 1.02;
				screenright /= 1.02;
				screentop /= 1.02;
				screenbotton /= 1.02;
				reshapeWindow(window, 1200, 600);
				*/	}
	else if(yoffset == -1){
		/*		if(screenleft >= -600.0f/1.02f)
				screenleft *= 1.02;
				if(screenright <= 600.0f/1.02f)
				screenright *= 1.02;
				if(screentop >= -300.0f/1.02f)
				screentop *= 1.02;
				if(screenbotton <= 300.0f/1.02f)
				screenbotton *= 1.02;
				reshapeWindow(window, 1200, 600);
				*/	}

}

static void cursor_position_callback(GLFWwindow* window, double xpos, double ypos)
{
	curx = ((screenright - screenleft)/1200.0f)*xpos + screenleft;
	cury = ((screenbotton - screentop)/600.0f)*ypos + screentop;
	/*	if(panning_state == 1){
		if(paninitx - curx < 0 && screenleft >= -600 + fabs(paninitx - curx)){
		screenleft -= fabs(paninitx - curx);
		screenright -= fabs(paninitx - curx);
		}
		if(paninitx - curx > 0 && screenright <= 600 - fabs(paninitx -curx)){
		screenleft += fabs(paninitx -curx);
		screenright += fabs(paninitx -curx);
		}
		if(paninity - cury < 0  && screentop >= -300 + fabs(paninity - cury)){
		screentop -= fabs(paninity - cury);
		screenbotton -= fabs(paninity - cury);
		}
		if(paninity - cury > 0 && screenbotton <= 300 - fabs(paninity -cury)){
		screentop += fabs(paninity - cury);
		screenbotton += fabs(paninity -cury);
		}
		reshapeWindow(window, 1200, 600);
		}
		*/
}


/* Executed when a mouse button is pressed/released */
void mouseButton (GLFWwindow* window, int button, int action, int mods)
{
	switch (button) {
		case GLFW_MOUSE_BUTTON_LEFT:
			if (action == GLFW_PRESS){
			}
			if (action == GLFW_RELEASE ){
			}
			break;
		case GLFW_MOUSE_BUTTON_RIGHT:
			if (action == GLFW_RELEASE) {
			}
			if(action == GLFW_PRESS){
			}
			break;
		default:
			break;
	}
}


/* Executed when window is resized to 'width' and 'height' */
/* Modify the bounds of the screen here in glm::ortho or Field of View in glm::Perspective */
void reshapeWindow (GLFWwindow* window, int width, int height)
{
	int fbwidth=width, fbheight=height;
	/* With Retina display on Mac OS X, GLFW's FramebufferSize
	   is different from WindowSize */
	glfwGetFramebufferSize(window, &fbwidth, &fbheight);

	GLfloat fov = 1.0f;

	// sets the viewport of openGL renderer
	glViewport (0, 0, (GLsizei) fbwidth, (GLsizei) fbheight);

	// set the projection matrixscreennearperspective
	/* glMatrixMode (GL_PROJECTION);
	   glLoadIdentity ();
	   gluPerspective (fov, (GLfloat) fbwidth / (GLfloat) fbheight, 0.1, 500.0); */
	// Store the projection matrix in a variable for future use
	// Perspective projection for 3D views
	Matrices.projection = glm::perspective (fov, (GLfloat) fbwidth / (GLfloat) fbheight, 0.1f, screenfar);

	// Ortho projection for 2D views
	//	screenleft = -screenleft, screenright = -screenright, screenbotton= - screenbotton, screentop = -screentop;
	//Matrices.projection = glm::ortho(screenleft, screenright, screenbotton, screentop, screennear, screenfar);
}



float camera_rotation_angle = 90;
VAO *temp, *background;
void createtemp(){
	static const GLfloat vertex_buffer_data [] = {
		-100, -100, -100,
		100, -100, -100,
		-100, 100, -100,

		100, -100, -100,
		-100, 100, -100,
		100, 100, -100,

		100, -100, -100,
		100, 100, -100,
		100, 100, 100,

		100, -100, -100,
		100, 100, 100,
		100, -100, 100,

		100, 100, 100,
		100, -100, 100,
		-100, -100, 100,

		100, 100, 100,
		-100, -100, 100,
		-100, 100, 100,

		-100, -100, 100,
		-100, 100, 100,
		-100, 100, -100,

		-100, -100, 100,
		-100, 100, -100,
		-100, -100, -100,

		-100, -100, -100,
		100, -100, -100,
		100, -100, 100,

		-100, -100, -100,
		100, -100, 100,
		-100, -100, 100,
		
		-100, 100, -100,
		100, 100, -100,
		100, 100, 100,

		-100, 100, -100,
		100, 100, 100,
		-100, 100, 100

	};
	static const GLfloat color_buffer_data [] ={
		1,0,0,
		1,0,0,
		1,0,0,

		1,0,0,
		1,0,0,
		1,0,0,

		0,1,0,
		0,1,0,
		0,1,0,

		0,1,0,
		0,1,0,
		0,1,0,

		0,0,1,
		0,0,1,
		0,0,1,
		
		0,0,1,
		0,0,1,
		0,0,1,

		1,1,1,
		1,1,1,
		1,1,1,

		1,1,1,
		1,1,1,
		1,1,1,

		1,1,0,
		1,1,0,
		1,1,0,

		1,1,0,
		1,1,0,
		1,1,0,

		0,1,1,
		0,1,1,
		0,1,1,

		0,1,1,
		0,1,1,
		0,1,1
	};
	temp = create3DObject(GL_TRIANGLES, 36 , vertex_buffer_data, color_buffer_data, GL_FILL);
}

void createBackground(GLuint textureID){
	static const GLfloat vertex_buffer_data[] = {
		-600, -300, 0,
		-600, 300, 0,
		600, -300, 0,

		-600, 300, 0,
		600, -300, 0,
		600, 300, 0
	};

	static const GLfloat texture_buffer_data[] = {
		0,0,
		0,1,
		1,0,

		0,1,
		1,0,
		1,1
	};

	background = create3DTexturedObject(GL_TRIANGLES, 6, vertex_buffer_data, texture_buffer_data, textureID, GL_FILL);
}

float dist = 200;
float angle = 0;
float zdist = 120;
void draw ()
{
	// clear the color and depth in the frame buffer
	glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);


	// use the loaded shader program
	// Don't change unless you know what you are doing
	glUseProgram (programID);

	// Eye - Location of camera. Don't change unless you are sure!!
	glm::vec3 eye ( 5*cos(camera_rotation_angle*M_PI/180.0f), 0, 5*sin(camera_rotation_angle*M_PI/180.0f) );
	// Target - Where is the camera looking at.  Don't change unless you are sure!!
	glm::vec3 target (0, 0, 0);
	// Up - Up vector defines tilt of camera.  Don't change unless you are sure!!
	glm::vec3 up (0, 1, 0);

	// Compute Camera matrix (view)
	//Matrices.view = glm::lookAt( eye, target, up ); // Rotating Camera for 3D
	//  Don't change unless you are sure!!
	Matrices.view = glm::lookAt(glm::vec3(0,dist * cos(angle * M_PI/180.0f),dist * sin(angle * M_PI/180.0f)), glm::vec3(0,0,0), glm::vec3(0,1,0)); // Fixed camera for 2D (ortho) in XY plane
	//Matrices.view = glm::lookAt(glm::vec3(200,-200,-300), glm::vec3(0,0,0), glm::vec3(0,1,0)); // Fixed camera for 2D (ortho) in XY plane
	camera_rotation_angle += 1;
	angle += 1;
	zdist += 1;
	zdist = min(zdist,500.0f);
	// Compute ViewProject matrix as view/camera might not be changed for this frame (basic scenario)
	//  Don't change unless you are sure!!
	glm::mat4 VP = Matrices.projection * Matrices.view;

	// Send our transformation to the currently bound shader, in the "MVP" uniform
	// For each model you render, since the MVP will be different (at least the M part)
	//  Don't change unless you are sure!!
	glm::mat4 MVP;	// MVP = Projection * View * Model


	//Displaying background using texture
	glUseProgram(textureProgramID);

	Matrices.model = glm::mat4(1.0f);
	MVP = VP * Matrices.model;
	glUniformMatrix4fv(Matrices.TexMatrixID, 1, GL_FALSE, &MVP[0][0]);
	glUniform1i(glGetUniformLocation(textureProgramID, "texSampler"), 0);
	//draw3DTexturedObject(background);


	glUseProgram (programID);
	// Load identity to model matrix
	Matrices.model = glm::mat4(1.0f);

	/* Render your scene */

	//glm::mat4 translateTriangle = glm::translate (glm::vec3(-2.0f, 0.0f, 0.0f)); // glTranslatef
	//glm::mat4 rotateTriangle = glm::rotate((float)(triangle_rotation*M_PI/180.0f), glm::vec3(0,0,1));  // rotate about vector (1,0,0)
	//glm::mat4 triangleTransform = translateTriangle * rotateTriangle;
	//  Matrices.model *= triangleTransform; 
	MVP = VP * Matrices.model; // MVP = p * V * M

	//  Don't change unless you are sure!!
	glUniformMatrix4fv(Matrices.MatrixID, 1, GL_FALSE, &MVP[0][0]);
	// draw3DObject draws the VAO given to it using current MVP matrix
	//  draw3DObject(triangle);

	// Pop matrix to undo transformations till last push matrix instead of recomputing model matrix
	// glPopMatrix ();

	Matrices.model = glm::mat4(1.0f);
	//	glm::mat4 scalePower = glm::scale(glm::vec3(power*6,1,1));
	//	glm::mat4 translatePower = glm::translate(glm::vec3(-400 - ( 90 - power * 3), -240, 0));
	//	Matrices.model *= ( translatePower * scalePower);
	MVP = VP * Matrices.model;
	glUniformMatrix4fv(Matrices.MatrixID, 1, GL_FALSE, &MVP[0][0]);
	draw3DObject(temp);

	// Render font on screen
	static int fontScale = 1;
	float fontScaleValue = 50 + 0.25*sinf(fontScale*M_PI/180.0f);
	glm::vec3 fontColor = glm::vec3(228.0f/255.0f,142.0f/255.0f,57.0f/255.0f);//getRGBfromHue (fontScale);



	// Use font Shaders for next part of code
	glUseProgram(fontProgramID);
	Matrices.view = glm::lookAt(glm::vec3(0,0,3), glm::vec3(0,0,0), glm::vec3(0,1,0)); // Fixed camera for 2D (ortho) in XY plane

	// Transform the text
	Matrices.model = glm::mat4(1.0f);
	glm::mat4 translateText = glm::translate(glm::vec3(400,-250,0));
	glm::mat4 scaleText = glm::scale(glm::vec3(fontScaleValue,fontScaleValue,fontScaleValue));
	glm::mat4 rotateText = glm::rotate((float)M_PI, glm::vec3(1,0,0));
	Matrices.model *= (translateText * scaleText * rotateText);
	MVP = Matrices.projection * Matrices.view * Matrices.model;
	// send font's MVP and font color to fond shaders
	glUniformMatrix4fv(GL3Font.fontMatrixID, 1, GL_FALSE, &MVP[0][0]);
	glUniform3fv(GL3Font.fontColorID, 1, &fontColor[0]);

	//string str = to_string(score);
	char str[10];
	// Render font
	GL3Font.font->Render("Hello");
}

/* Initialise glfw window, I/O callbacks and the renderer to use */
/* Nothing to Edit here */
GLFWwindow* initGLFW (int width, int height)
{
	GLFWwindow* window; // window desciptor/handle

	glfwSetErrorCallback(error_callback);
	if (!glfwInit()) {
		exit(EXIT_FAILURE);
	}

	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	window = glfwCreateWindow(width, height, "Adventura", NULL, NULL);

	if (!window) {
		glfwTerminate();
		exit(EXIT_FAILURE);
	}

	glfwMakeContextCurrent(window);
	gladLoadGLLoader((GLADloadproc) glfwGetProcAddress);
	glfwSwapInterval( 1 );

	/* --- register callbacks with GLFW --- */

	/* Register function to handle window resizes */
	/* With Retina display on Mac OS X GLFW's FramebufferSize
	   is different from WindowSize */
	glfwSetFramebufferSizeCallback(window, reshapeWindow);
	glfwSetWindowSizeCallback(window, reshapeWindow);

	/* Register function to handle window close */
	glfwSetWindowCloseCallback(window, quit);

	/* Register function to handle keyboard input */
	glfwSetKeyCallback(window, keyboard);      // general keyboard input
	glfwSetCharCallback(window, keyboardChar);  // simpler specific character handling

	/* Register function to handle mouse click */
	glfwSetMouseButtonCallback(window, mouseButton);  // mouse button clicks
	glfwSetCursorPosCallback(window, cursor_position_callback);
	glfwSetScrollCallback(window, scroll_callback);
	return window;
}

/* Initialize the OpenGL rendering properties */
/* Add all the models to be created here */
void initGL (GLFWwindow* window, int width, int height)
{
	// Load Textures
	// Enable Texture0 as current texture memory
	glActiveTexture(GL_TEXTURE0);

	// load an image file directly as a new OpenGL texture
	// GLuint texID = SOIL_load_OGL_texture ("beach.png", SOIL_LOAD_AUTO, SOIL_CREATE_NEW_ID, SOIL_FLAG_TEXTURE_REPEATS); // Buggy for OpenGL3
	GLuint textureID = createTexture("background.png");

	// check for an error during the load process
	if(textureID == 0 )
		cout << "SOIL loading error: '" << SOIL_last_result() << "'" << endl;

	// Create and compile our GLSL program from the texture shaders
	textureProgramID = LoadShaders( "TextureRender.vert", "TextureRender.frag" );
	// Get a handle for our "MVP" uniform
	Matrices.TexMatrixID = glGetUniformLocation(textureProgramID, "MVP");


	/* Objects should be created before any other gl function and shaders */
	// Create the models
	// Generate the VAO, VBOs, vertices data & copy into the array buffer
	createBackground (textureID);
	createtemp();
	//createCatapult2();

	// Create and compile our GLSL program from the shaders
	programID = LoadShaders( "Sample_GL.vert", "Sample_GL.frag" );
	// Get a handle for our "MVP" uniform
	Matrices.MatrixID = glGetUniformLocation(programID, "MVP");


	reshapeWindow (window, width, height);

	// Background color of the scene
	glClearColor(156.0/255.0f,205.0f/255.0f,237.0f/255.0f,0.0f);// (0.3f, 0.3f, 0.3f, 0.0f); // R, G, B, A
	glClearDepth (1.0f);

	glEnable (GL_DEPTH_TEST);
	glDepthFunc (GL_LEQUAL);

	// Initialise FTGL stuff
	//const char* fontfile = "UpsideDown.ttf";
	const char* fontfile = "arial.ttf";
	GL3Font.font = new FTExtrudeFont(fontfile); // 3D extrude style rendering

	if(GL3Font.font->Error())
	{
		cout << "Error: Could not load font `" << fontfile << "'" << endl;
		glfwTerminate();
		exit(EXIT_FAILURE);
	}

	// Create and compile our GLSL program from the font shaders
	fontProgramID = LoadShaders( "fontrender.vert", "fontrender.frag" );
	GLint fontVertexCoordAttrib, fontVertexNormalAttrib, fontVertexOffsetUniform;
	fontVertexCoordAttrib = glGetAttribLocation(fontProgramID, "vertexPosition");
	fontVertexNormalAttrib = glGetAttribLocation(fontProgramID, "vertexNormal");
	fontVertexOffsetUniform = glGetUniformLocation(fontProgramID, "pen");
	GL3Font.fontMatrixID = glGetUniformLocation(fontProgramID, "MVP");
	GL3Font.fontColorID = glGetUniformLocation(fontProgramID, "fontColor");

	GL3Font.font->ShaderLocations(fontVertexCoordAttrib, fontVertexNormalAttrib, fontVertexOffsetUniform);
	GL3Font.font->FaceSize(1);
	GL3Font.font->Depth(0);
	GL3Font.font->Outset(0, 0);
	GL3Font.font->CharMap(ft_encoding_unicode);

	cout << "VENDOR: " << glGetString(GL_VENDOR) << endl;
	cout << "RENDERER: " << glGetString(GL_RENDERER) << endl;
	cout << "VERSION: " << glGetString(GL_VERSION) << endl;
	cout << "GLSL: " << glGetString(GL_SHADING_LANGUAGE_VERSION) << endl;
}

int main (int argc, char** argv)
{
	int width = 1200;
	int height = 600;

	GLFWwindow* window = initGLFW(width, height);

	initGL (window, width, height);

	double last_update_time = glfwGetTime(), current_time;


	pid = fork();
	if(pid==0){
		mpg123_handle *mh;
		unsigned char *buffer;
		size_t buffer_size;
		size_t done;
		int err;

		int driver;
		ao_device *dev;

		ao_sample_format format;
		int channels, encoding;
		long rate;


		/* initializations */
		ao_initialize();
		driver = ao_default_driver_id();
		mpg123_init();
		mh = mpg123_new(NULL, &err);
		buffer_size = mpg123_outblock(mh);
		buffer = (unsigned char*) malloc(buffer_size * sizeof(unsigned char));

		/* open the file and get the decoding format */
		mpg123_open(mh, "trial.mp3");
		mpg123_getformat(mh, &rate, &channels, &encoding);

		/* set the output format and open the output device */
		format.bits = mpg123_encsize(encoding) * BITS;
		format.rate = rate;
		format.channels = channels;
		format.byte_format = AO_FMT_NATIVE;
		format.matrix = 0;
		dev = ao_open_live(driver, &format, NULL);

		/* decode and play */
		char *p =(char *)buffer;
		while (mpg123_read(mh, buffer, buffer_size, &done) == MPG123_OK)
			ao_play(dev, p, done);

		/* clean up */
		free(buffer);
		ao_close(dev);
		mpg123_close(mh);
		mpg123_delete(mh);
		mpg123_exit();
		ao_shutdown();
		_exit(0);
	}

	int level = 1;
	string line;
	ifstream levelfile("1.txt");
	if(levelfile.is_open()){
		while(getline(levelfile, line)){
			cout<<line<<endl;
		}
		levelfile.close();
	}
	else
		cout<<"Unable to open file";
	/* Draw in loop */
	while (!glfwWindowShouldClose(window)) {

		/*		if(panleft == 1 && screenleft >= -600 + 5){
				screenleft -= 5;
				screenright -= 5;
				}
				if(panright == 1 && screenright <= 600 - 5){
				screenleft += 5;
				screenright += 5;
				}
				if(panup == 1 && screentop >= -300 + 5){
				screentop -= 5;
				screenbotton -= 5;
				}
				if(pandown == 1 && screenbotton <= 300 - 5){
				screentop += 5;
				screenbotton += 5;
				}

				if(zoominstate == 1 && screenright-screenleft > 800) {
				screenleft /= 1.02;
				screenright /= 1.02;
				screentop /= 1.02;
				screenbotton /= 1.02;
				}
				if(zoomoutstate == 1 && screenright - screenleft < 1200) {
				if(screenleft >= -600.0f/1.02f)
				screenleft *= 1.02;
				if(screenright <= 600.0f/1.02f)
				screenright *= 1.02;
				if(screentop >= -300.0f/1.02f)
				screentop *= 1.02;
				if(screenbotton <= 300.0f/1.02f)
				screenbotton *= 1.02;
				}
				if(zoominstate == 1 || zoomoutstate == 1 || panleft == 1 || panright == 1 || panup == 1 || pandown == 1)
				reshapeWindow(window, width, height);
				*/		// OpenGL Dramands
		draw();

		// Swap Frame Buffer in double buffering
		glfwSwapBuffers(window);

		// Poll for Keyboard and mouse events
		glfwPollEvents();

		// Control based on time (Time based transformation like 5 degrees rotation every 0.5s)
		current_time = glfwGetTime(); // Time in seconds
		if ((current_time - last_update_time) >= 0.5) { // atleast 0.5s elapsed since last frame
			// do something every 0.5 seconds ..
			last_update_time = current_time;
		}
	}

	glfwTerminate();
	exit(EXIT_SUCCESS);
}
