//
// RoboSimo - The RoboSumo simulator - Ted Burke - 26-9-2009
//
// Programs written in C can access this simulator over
// the network and control a virtual robot within the
// simulated environment.
//

// Header files
#include <windows.h>
#include <stdio.h>
#include <time.h>
#include <GL/glu.h>
#include "glut.h"

// My header files
#include "shared.h"

// GLUT callback functions
void update(void);
void display(void);
void reshape(int, int);
void keyboard(unsigned char key, int x, int y);
void keyboardSpecial(int key, int x, int y);
void mouse(int, int, int, int);
void mouseDrag(int, int);

// Define the value of pi
const double pi = 3.14159;

// Remember when the scene was last updated (in clock ticks)
clock_t last_time;

// GLUT window identifier
static int window;

// Orthographic projection dimensions
double ortho_left, ortho_right, ortho_bottom, ortho_top;
GLint orthographic_projection = 0;

// Camera position for perspective view
GLfloat camera_latitude = 50; // degrees "south" of vertical
GLfloat camera_longitude = 0; // degrees "east" of reference point
GLfloat camera_distance = 4; // distance from centre point of table

// Global flags
int fullscreen = 0; // to be set to 1 for fullscreen mode

// Global identifier for arean floor texture
static GLuint texName;
#define texImageWidth 512
#define texImageHeight 512
static GLubyte texImage[texImageHeight][texImageWidth][3];

// definition for network thread function
int p1 = 1;
int p2 = 2;
extern DWORD WINAPI network_thread(LPVOID);
HANDLE hNetworkThread1, hNetworkThread2;

// This string will be set to the network address
// info to be displayed on the screen.
extern char network_address_display_string[];
double x_networkAddressText, y_networkAddressText;

// Function prototypes
void renderBitmapString(float, float, float, void *, char *);
void initialise_robots();

int main(int argc, char **argv)
{
	// Start GLUT
	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE | GLUT_DEPTH);

	glutInitWindowSize(640,400);
	glutInitWindowPosition(100,100);
	window = glutCreateWindow("RoboSimo");

	// Go fullscreen if flag is set
	if (fullscreen) glutFullScreen();
	
	// Register callback functions
	glutIdleFunc(update);
	glutKeyboardFunc(keyboard);
	glutSpecialFunc(keyboardSpecial);
	glutMouseFunc(mouse);
	glutMotionFunc(mouseDrag);
	glutDisplayFunc(display);
	glutReshapeFunc(reshape);
	
	// Set the background colour to green
	glClearColor(0, 1, 0, 0);
	
	// Load floor pattern from bmp file
	FILE *texture_file = fopen("floor.bmp", "r");
	fread(texImage, 1, 0x36, texture_file); // Read bitmap header - assume it's 0x36 bytes long
	fread(texImage, 3, texImageWidth*texImageHeight, texture_file);
	fclose(texture_file);
	
	// Convert from BGR to RGB
	int x, y;
	GLubyte temp;
	for (y = 0 ; y < texImageHeight ; ++y)
	{
		for (x = 0 ; x < texImageWidth ; ++x)
		{
			temp = texImage[y][x][0];
			texImage[y][x][0] = texImage[y][x][2];
			texImage[y][x][2] = temp;
		}
	}
	
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glGenTextures(1, &texName);
	glBindTexture(GL_TEXTURE_2D, texName);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, texImageWidth, texImageHeight,
					0, GL_RGB, GL_UNSIGNED_BYTE, texImage);

	// Initialise robots
	initialise_robots();
	
	// Launch network threads (one for each player, ports 4009 and 4010)
	hNetworkThread1 = CreateThread(NULL, 0, network_thread, (LPVOID)(&p1), 0, NULL);
	hNetworkThread2 = CreateThread(NULL, 0, network_thread, (LPVOID)(&p2), 0, NULL);
	
	// Enter the main event loop
	glutMainLoop();

	// The program will actually never reach this point
	// because glutMainLoop() never returns before the
	// program exits.
	return 0;
}

void initialise_robots()
{
	// Initialise robots' states
	int n;
	for (n=0 ; n<2 ; ++n)
	{
		robot[n].active = 0; // inactive until a competitor connects over the network
		robot[n].name[20];
		robot[n].w = 0.16; // 16cm wide
		robot[n].l = 0.20; // 20cm long
		robot[n].h = 0.10; // 10cm high

		robot[n].LATA = 0;
		robot[n].LATB = 0;
		robot[n].LATC = 0;
		robot[n].LATD = 0;
	
		// PWM output registers
		robot[n].CCPR1L = 255;
		robot[n].CCPR2L = 255;
		
		// Analog inputs
		robot[n].AN[0] = 0;
		robot[n].AN[1] = 0;
		robot[n].AN[2] = 0;
		robot[n].AN[3] = 0;
		robot[n].AN[4] = 0;
		robot[n].AN[5] = 0;
		robot[n].AN[6] = 0;
		robot[n].AN[7] = 0;
	}
		
	// Random angle offset
	double max_random_angle_offset = 0;
	
	// Load robot_positions.txt to set starting positions
	FILE *robot_positions_file = fopen("robot_positions.txt", "r");
	char word[20];
	while(1)
	{
		fscanf(robot_positions_file, "%s", word);
		if (strcmp(word, "END") == 0) break;
		else if (strcmp(word, "ROBOT_0_X") == 0)
		{
			fscanf(robot_positions_file, "%lf", &robot[0].x);
		}
		else if (strcmp(word, "ROBOT_0_Y") == 0)
		{
			fscanf(robot_positions_file, "%lf", &robot[0].y);
		}
		else if (strcmp(word, "ROBOT_0_ANGLE") == 0)
		{
			fscanf(robot_positions_file, "%lf", &robot[0].angle);
		}
		else if (strcmp(word, "ROBOT_1_X") == 0)
		{
			fscanf(robot_positions_file, "%lf", &robot[1].x);
		}
		else if (strcmp(word, "ROBOT_1_Y") == 0)
		{
			fscanf(robot_positions_file, "%lf", &robot[1].y);
		}
		else if (strcmp(word, "ROBOT_1_ANGLE") == 0)
		{
			fscanf(robot_positions_file, "%lf", &robot[1].angle);
		}
		else if (strcmp(word, "MAX_RANDOM_ANGLE_OFFSET") == 0)
		{
			fscanf(robot_positions_file, "%lf", &max_random_angle_offset);
		}
	}

	// Add random angle offsets
	robot[0].angle += max_random_angle_offset * (2 * (rand() / (double)RAND_MAX) - 1);
	robot[1].angle += max_random_angle_offset * (2 * (rand() / (double)RAND_MAX) - 1);

	fclose(robot_positions_file);
}

// This function obtains the background colour (actually intensity between 0 and 255)
// at a specified OpenGL coordinate
GLubyte get_colour(GLfloat x, GLfloat y)
{
	// NB The loaded floor texture is mapped onto the square area from top left
	// at (-0.7, 0.7) to bottom right at (0.7, -0.7).
	//
	// NB The first line in the bitmap data is actually the bottom row of pixels
	// in the image.
	
	// First translate x and y to the range 0 to 1
	x = (x + 0.7) / 1.4;
	y = (y + 0.7) / 1.4;
	if (x > 1) x = 1;
	if (x < 0) x = 0;
	if (y > 1) y = 1;
	if (y < 0) y = 0;

	// Now identify the corresponding pixel coordinate
	GLint px, py;
	px = (GLint)(texImageWidth * x);
	py = (GLint)(texImageHeight * y);
	
	// Finally, calculate and return the average colour component value at that point
	return (GLubyte)((texImage[py][px][0] + texImage[py][px][1] + texImage[py][px][2]) / 3.0);
}

void update()
{
	// Check if program should exit
	if (program_exiting)
	{
		// Clean up, then exit
		glutDestroyWindow(window);
		WaitForSingleObject(hNetworkThread1, 1000); // Wait up to 1 second for network thread to exit
		WaitForSingleObject(hNetworkThread2, 1000); // Wait up to 1 second for network thread to exit
		CloseHandle(hNetworkThread1);
		CloseHandle(hNetworkThread2);
		exit(0);
	}
	
	// Calculate time elapsed since last update
	double tau;
	clock_t new_time;
	new_time = clock();
	if (new_time > last_time) tau = (new_time - last_time) / (double)CLOCKS_PER_SEC;
	else tau = 0; // Just in case clock value reaches max value and wraps around
	last_time = new_time;
	
	// Update robot positions
	GLfloat x1, y1, x2, y2;
	int n;
	for (n=0 ; n<=1 ; ++n)
	{
		// Update wheel velocities
		robot[n].v1 = 0;
		robot[n].v2 = 0;
		if (robot[n].LATD & 0x02) robot[n].v1 += 1;
		if (robot[n].LATD & 0x01) robot[n].v1 -= 1;
		robot[n].v1 *= (robot[n].CCPR1L / 1000.0);
		if (robot[n].LATD & 0x08) robot[n].v2 += 1;
		if (robot[n].LATD & 0x04) robot[n].v2 -= 1;
		robot[n].v2 *= (robot[n].CCPR2L / 1000.0);
		
		// Left wheel position: x1, y1. Left wheel velocity: v1.
		// Right wheel position: x2, y2. Left wheel velocity: v2.
		x1 = robot[n].x - (robot[n].w/2.0)*sin(robot[n].angle) + tau*robot[n].v1*cos(robot[n].angle);
		y1 = robot[n].y + (robot[n].w/2.0)*cos(robot[n].angle) + tau*robot[n].v1*sin(robot[n].angle);
		x2 = robot[n].x + (robot[n].w/2.0)*sin(robot[n].angle) + tau*robot[n].v2*cos(robot[n].angle);
		y2 = robot[n].y - (robot[n].w/2.0)*cos(robot[n].angle) + tau*robot[n].v2*sin(robot[n].angle);
		robot[n].x = (x1 + x2) / 2.0;
		robot[n].y = (y1 + y2) / 2.0;
		
		if (x2 == x1)
		{
			// robot pointing (close to) straight up or down
			if (y2 < y1) robot[n].angle = 0;
			else robot[n].angle = pi;
		}
		else
		{
			// Calculate updated orientation from new wheel positions
			robot[n].angle = (pi/2.0) + atan2(y2-y1, x2-x1);
		}
		
		// Update sensor readings
		// First, calculate offset vector from centre of robot
		// to mid-right-side v1 = (x1, y1) and from centre of
		// robot to front-centre-point v2 = (x2, y2)
		GLfloat x = robot[n].x;
		GLfloat y = robot[n].y;
		GLfloat x1 = (robot[n].w/2.0)*sin(robot[n].angle);
		GLfloat y1 = -(robot[n].w/2.0)*cos(robot[n].angle);
		GLfloat x2 = (robot[n].l/2.0)*cos(robot[n].angle);
		GLfloat y2 = (robot[n].l/2.0)*sin(robot[n].angle);
		robot[n].AN[0] = get_colour(x - x1 + x2, y - y1 + y2); // front left
		robot[n].AN[1] = get_colour(x + x1 + x2, y + y1 + y2);; // front right
		robot[n].AN[2] = get_colour(x - x1 - x2, y - y1 - y2);; // back left
		robot[n].AN[3] = get_colour(x + x1 - x2, y + y1 - y2);; // back right
	}
	
	// Request redrawing of scene
	glutPostRedisplay();
}

void reshape(int window_width, int window_height)
{
	int text_bar_height = 25;
	
	// Set viewport to new window size
	glViewport(0, 0, window_width, window_height);
	
	// Calculate dimensions for orthographic OpenGL projection
	if (window_width > window_height - text_bar_height)
	{
		ortho_top = 0.7;
		ortho_bottom = -0.7 * (window_height + 2.0*text_bar_height) / (double)window_height;
		ortho_right = 0.7 * (window_width/(double)(window_height-text_bar_height));
		ortho_left = -ortho_right;
	}
	else
	{
		ortho_right = 0.7;
		ortho_left = -ortho_right;
		ortho_top = 0.7 * ((window_height-text_bar_height)/(double)window_width);
		ortho_bottom = -ortho_top * (window_height + 2.0 * text_bar_height) / (double)window_height;
	}
	
	// Update position of network address text
	x_networkAddressText = ortho_left; // NB string starts with a couple of spaces, so ok to place at left edge
	y_networkAddressText = -0.7;
}

void display()
{
	// Clear the background
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	// Select appropriate projection
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	if (orthographic_projection == 1)
	{
		// Specify orthographic projection
		glOrtho(ortho_left, ortho_right, ortho_bottom, ortho_top, -1, 100);
	}
	else
	{
		// Specify perspective projection - fovy, aspect, zNear, zFar
		gluPerspective(30, 1, 1, 100);
	}
	
	// Render bitmap on arena floor
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	gluLookAt(0, 0, camera_distance, 0, 0, 0, 0, 1, 0);
	if (orthographic_projection == 0)
	{
		glRotatef(-camera_latitude, 1, 0, 0);
		glRotatef(-camera_longitude, 0, 0, 1);
	}
	
	glColor3d(1,1,1);
	glEnable(GL_TEXTURE_2D);
	glBegin(GL_QUADS);
	glTexCoord2f(0.0, 1.0);
	glVertex3f(-0.7, 0.7, 0.0);
	glTexCoord2f(1.0, 1.0);
	glVertex3f(0.7, 0.7, 0.0);
	glTexCoord2f(1.0, 0.0);
	glVertex3f(0.7, -0.7, 0.0);
	glTexCoord2f(0.0, 0.0);
	glVertex3f(-0.7, -0.7, 0.0);
	glEnd();
	glDisable(GL_TEXTURE_2D);

	// Draw robots
	glEnable(GL_DEPTH_TEST);
	int n;
	for (n=0 ; n<=1 ; ++n)
	{
		// rectangular robot body
		if (n == 0) glColor3f(1.0, 0.0, 0.0);
		else glColor3f(0.0, 0.0, 1.0);
		glLoadIdentity();
		gluLookAt(0, 0, camera_distance, 0, 0, 0, 0, 1, 0);
		if (orthographic_projection == 0)
		{
			glRotatef(-camera_latitude, 1, 0, 0);
			glRotatef(-camera_longitude, 0, 0, 1);
		}
		glTranslatef(robot[n].x, robot[n].y, robot[n].h/2.0);
		glRotatef(robot[n].angle * (180.0/pi), 0.0, 0.0, 1.0);
		glScalef(robot[n].l, robot[n].w, robot[n].h);
		glutSolidCube(1.0);
		// Simple indicator which end of the robot is the front
		glColor3f(1.0, 1.0, 1.0);
		glLoadIdentity();
		gluLookAt(0, 0, camera_distance, 0, 0, 0, 0, 1, 0);
		if (orthographic_projection == 0)
		{
			glRotatef(-camera_latitude, 1, 0, 0);
			glRotatef(-camera_longitude, 0, 0, 1);
		}
		glTranslatef(robot[n].x, robot[n].y, robot[n].h);
		glRotatef(robot[n].angle * (180.0/pi), 0.0, 0.0, 1.0);
		glScalef(robot[n].l, robot[n].w, robot[n].w);
		glTranslatef(-0.25, 0.0, 0.0);
		glRotatef(90.0, 0.0, 1.0, 0.0);
		glutSolidCone(0.25, 0.5, 4, 4);
	}
	glDisable(GL_DEPTH_TEST);
	
	// Specify OpenGL projection
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(ortho_left, ortho_right, ortho_bottom, ortho_top, -1, 100);

	// Draw text information
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glColor3f(0.0, 0.0, 0.0);
	renderBitmapString(x_networkAddressText, y_networkAddressText, 0.0,
						GLUT_BITMAP_HELVETICA_18, network_address_display_string);
	
	// Swap back buffer to screen
	glutSwapBuffers();
}

void keyboard(unsigned char key, int x, int y)
{
	if (key == 'q') program_exiting = 1; // Set exiting flag
	if (key == ' ') initialise_robots();
	if (key == 'v') orthographic_projection = (orthographic_projection) ? 0 : 1;
}

void keyboardSpecial(int key, int x, int y)
{
	if (key == GLUT_KEY_UP) camera_distance -= 0.1;
	if (key == GLUT_KEY_DOWN) camera_distance += 0.1;
	if (key == GLUT_KEY_LEFT) camera_longitude -= 10.0;
	if (key == GLUT_KEY_RIGHT) camera_longitude += 10.0;
}

int mouse_previous_x, mouse_previous_y;

void mouse(int button, int state, int x, int y)
{
	if ((button == GLUT_LEFT_BUTTON) && (state == GLUT_DOWN))
	{
		mouse_previous_x = x;
		mouse_previous_y = y;
	}
}

void mouseDrag(int x, int y)
{
	camera_latitude -= (y - mouse_previous_y)/2.0;
	camera_longitude -= (x - mouse_previous_x)/2.0;
	mouse_previous_x = x;
	mouse_previous_y = y;
	
	if (camera_latitude > 90.0) camera_latitude = 90.0;
	if (camera_latitude < 0.0) camera_latitude = 0.0;
	if (camera_longitude > 360.0) camera_longitude -= 360.0;
	if (camera_longitude < 0.0) camera_longitude += 360.0;
}

// This function renders a string (some text) on the screen at a specified position
void renderBitmapString(float x, float y, float z, void *font, char *text)
{
	// Variable to count through the string's character
	int n = 0;
	
	// Print the characters of the string one by one
	glRasterPos3f(x, y, z);
	while(text[n] != '\0')
	{
		// Render a character
		glutBitmapCharacter(font, text[n]);
		n = n + 1;
	}
}
