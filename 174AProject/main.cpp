// ------------------------
// GLUT harness v. 1.0
// ------------------------

#include <stdlib.h>
#include <GL/glew.h>
#include <stdio.h>
#include "Angel.h"
#include <ctime>
#include "SOIL.h"
#include <math.h>

#if defined(__APPLE__)
#include <GLUT/glut.h>
#else
#include <GL/glut.h>
#endif

#define FPS 30
#define DEFAULTSPEED 50
#define MAXSPEED 200
GLfloat FORWARDSPEED = DEFAULTSPEED;//32; // Units/second
clock_t begin_time;
unsigned long highscore = 0;

// Function declarations
GLuint init_shaders(const char *vshadername, const char *fshadername);
void init_sphere(struct sphere &Sphere);
void init_cylinder(struct cylinder &Cylinder);
//void triangle( const vec4& a, const vec4&b, const vec4& c, vec4 points[3], vec3 normals[3] );
//vec4 unit( const vec4& p);
//void divide_triangle( const vec4& a, const vec4& b, const vec4& c, int count, vec4** ppoints, vec3** pnormals);
//void tetrahedron(int count, vec4 *points, vec3 *normals);
float distance(vec4 v1, vec4 v2);
void cylinder(int segments, vec4 *points, vec3 *normals);

// Perspective matrix definition
mat4 perspectiveM = Perspective(45, 16/9, 0.5, 500.0);
#define CAMPOSZ 35
mat4 worldTocamM = Translate(-vec4(0.0, 0.0, CAMPOSZ, 1.0));

#define CYLINDERSCALEXY 10
#define CYLINDERSCALEZ 20

// Player sphere parameters
#define NUMSPHERETIMESTODIVIDE 5
#define NUMSPHERETRIANGLES 4096 // 4 ^ (NUMTIMESTODIVIDESPHERE + 1)
#define NUMSPHEREVERTICES (3 * NUMSPHERETRIANGLES)
#define SPHEREVSHADER "sphere_vshader.glsl"
#define SPHEREFSHADER "sphere_fshader.glsl"

#define SPHERESCALEXYZ 1
#define SPHERESTARTDEGREES 270
#define SPHEREMOVEDELTA 10 // degrees
#define SPHEREMOVERADIUS (CYLINDERSCALEXY - SPHERESCALEXYZ)
#define SPHERESTARTPOSZ 0
	// Shading parameters
#define SPHEREAMBIENT 0.25
#define SPHEREDIFFUSE 0.5
#define SPHERESPECULAR 1.0
#define SPHERESHININESS 2.0

struct sphere {
	GLuint varray;
	GLuint buffer;
	GLuint program;
	GLfloat ScaleXYZ;
	mat4 scaleM;
	GLuint index_objToworldM;
	vec4 worldPosition;
	float posTheta; // degrees
	//
	GLuint index_lightPosition;
	vec4 ambientProduct;
	vec4 diffuseProduct;
	vec4 specularProduct;

GLint StartDegrees;
GLint MoveDelta;
GLfloat MoveRadius;
GLfloat StartPosZ;
GLfloat Ambient;
GLfloat Diffuse;
GLfloat Specular;
GLfloat Shininess;
	vec4 points[NUMSPHEREVERTICES];
	vec3 normals[NUMSPHEREVERTICES];
	vec2 texCoords[NUMSPHEREVERTICES];

	sphere() {
		ScaleXYZ = SPHERESCALEXYZ;
		StartDegrees = SPHERESTARTDEGREES;
		MoveDelta = SPHEREMOVEDELTA;
		MoveRadius = CYLINDERSCALEXY - SPHERESCALEXYZ;
		StartPosZ = SPHERESTARTPOSZ;
		// Shading parameters
		Ambient = SPHEREAMBIENT;
		Diffuse = SPHEREDIFFUSE;
		Specular = SPHERESPECULAR;
		Shininess = SPHERESHININESS;
	}
} PlayerSphere;

#define OBSTACLESCALEXYZ 1.5
#define OBSTACLEAMBIENT 0.01
#define OBSTACLEDIFFUSE 0.01
#define OBSTACLESPECULAR 0.1
#define OBSTACLESHININESS 1
#define OBSTACLEPROB 2 // 0.5 probability
#define OBSTACLEMOVERADIUS (CYLINDERSCALEXY - OBSTACLESCALEXYZ)
#define BEGINNINGNUMOBSTACLE 4
#define MAXNUMOBSTACLE 16
GLint numObstacle = BEGINNINGNUMOBSTACLE;
GLint numObstacleIncreases = 1;
struct obstacle : sphere {
	bool** exists;
	vec4** positions;
	obstacle() {
		ScaleXYZ = OBSTACLESCALEXYZ;
		// Shading parameters
		Ambient = OBSTACLEAMBIENT;
		Diffuse = OBSTACLEDIFFUSE;
		Specular = OBSTACLESPECULAR;
		Shininess = OBSTACLESHININESS;
		MoveRadius = OBSTACLEMOVERADIUS;
	}
} Obstacle;

// Tunnel parameters
#define NUMCYLINDERSEGMENTS 100
#define NUMCYLINDERVERTICES (NUMCYLINDERSEGMENTS * 6)
#define CYLINDERVSHADER "cylinder_vshader.glsl"
#define CYLINDERFSHADER "cylinder_fshader.glsl"
struct cylinder {
	GLuint varray;
	GLuint buffer;
	GLuint program;
	mat4 scaleM;

#define NUMTUNNELSECTIONS 16
	// X and Y positions are always (0,0), so only need to store Z
	float tunnelSectionZPositions[NUMTUNNELSECTIONS];
	int firstTunnelSection; // Index of first tunnel section to draw
	mat4 objToworldMs[NUMTUNNELSECTIONS];
	GLuint index_objToworldM;

	// Shading parameters
	GLuint index_lightPosition;
#define TUNNELAMBIENT 0.5
#define TUNNELDIFFUSE 1.0
#define TUNNELSPECULAR 1.0
	float shininess;
#define TUNNELSHININESS 5.0
	vec4 ambientProduct;
	vec4 diffuseProduct;
	vec4 specularProduct;
	obstacle Obstacles[NUMTUNNELSECTIONS];
} Tunnel;

// Window coordinates
int windowWidth = 640;

// Input mode parameters
bool mouseInput = false;
double mouseSphereDelta = 0;

GLuint textures[4];


void initPlayerSphere() {
	PlayerSphere.posTheta = PlayerSphere.StartDegrees;
	float radians = PlayerSphere.posTheta * M_PI / 180.0;
	float x = cosf(radians);
	float y = sinf(radians);
	PlayerSphere.worldPosition = vec4(x*PlayerSphere.MoveRadius, y*PlayerSphere.MoveRadius, PlayerSphere.StartPosZ, 1.0);

	init_sphere(PlayerSphere);

	PlayerSphere.index_lightPosition = glGetUniformLocation(PlayerSphere.program, "lightPosition");
	glUniform4fv(
		PlayerSphere.index_lightPosition,
		1,
		vec4( 0, 0, CAMPOSZ/3, 1.0));
}

void changeObstacleNum(int numberOfObstacles) {
	vec4 **newpositions = new vec4 *[NUMTUNNELSECTIONS];
	for (int i = 0; i < NUMTUNNELSECTIONS; i++)
		newpositions[i] = new vec4[numberOfObstacles];

	bool **newexists = new bool *[NUMTUNNELSECTIONS];
	for (int i = 0; i < NUMTUNNELSECTIONS; i++)
		newexists[i] = new bool[numberOfObstacles];

	for (int i = 0; i < NUMTUNNELSECTIONS; i++)
		for (int j = 0; j < numberOfObstacles; j++)
			newexists[i][j] = false;

	if (Obstacle.positions != NULL && Obstacle.exists != NULL) 
		for (int i = 0; i < NUMTUNNELSECTIONS; i++)
			for (int j = 0; j < numObstacle; j++) {
					if (j >= numberOfObstacles)
						break;
					newexists[i][j] = Obstacle.exists[i][j];
					newpositions[i][j] = Obstacle.positions[i][j];
				}

	numObstacle = numberOfObstacles;

	delete [] Obstacle.positions;
	delete [] Obstacle.exists;

	Obstacle.positions = newpositions;
	Obstacle.exists = newexists;
}

void initObstacle(int numberOfObstacles) {
	changeObstacleNum(BEGINNINGNUMOBSTACLE);

	init_sphere(Obstacle);

	Obstacle.index_lightPosition = glGetUniformLocation(Obstacle.program, "lightPosition");
	glUniform4fv(
		Obstacle.index_lightPosition,
		1,
		PlayerSphere.worldPosition);
}

void init()
{
	begin_time = clock();
	glEnable(GL_DEPTH_TEST);
	initPlayerSphere();
	initObstacle(numObstacle);
	init_cylinder(Tunnel);
}

void initGlut(int& argc, char** argv)
{
	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_DEPTH | GLUT_DOUBLE | GLUT_RGBA);
	glutInitWindowPosition(100, 50);
	glutInitWindowSize(640, 480);
	glutCreateWindow("Tunnel Dodge");
}

// Called when the window needs to be redrawn.
void callbackDisplay()
{
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glBindVertexArray(Tunnel.varray);
	glUseProgram(Tunnel.program);
	glUniform4fv(Tunnel.index_lightPosition,
		1,
		PlayerSphere.worldPosition);

	glBindTexture(GL_TEXTURE_2D, textures[2]);
	for(int i=0; i < NUMTUNNELSECTIONS; i++)
	{
		glUniformMatrix4fv(
			Tunnel.index_objToworldM,
			1,
			GL_TRUE,
			Tunnel.objToworldMs[i]);
		glDrawArrays(GL_TRIANGLES, 0, NUMCYLINDERVERTICES);
	}

	glBindVertexArray(Obstacle.varray);
	glUseProgram(Obstacle.program);
	
	glUniform4fv(Obstacle.index_lightPosition,
					1, 
					PlayerSphere.worldPosition);

	glBindTexture(GL_TEXTURE_2D, textures[1]);
	for (int i=0; i < NUMTUNNELSECTIONS;i++)
		for (int j=0; j < numObstacle; j++)
			if (Obstacle.exists[i][j]) {
				glUniformMatrix4fv(
					Obstacle.index_objToworldM,
					1, GL_TRUE,
					Translate(Obstacle.positions[i][j]) * Obstacle.scaleM );

				glDrawArrays(GL_TRIANGLES, 0, NUMSPHEREVERTICES);
			}

	glBindVertexArray(PlayerSphere.varray);
	glUseProgram(PlayerSphere.program);
	glUniformMatrix4fv(PlayerSphere.index_objToworldM,
		1,
		GL_TRUE,
		Translate(PlayerSphere.worldPosition) *
		PlayerSphere.scaleM);
	glBindTexture(GL_TEXTURE_2D, textures[0]);
	glDrawArrays(GL_TRIANGLES, 0, NUMSPHEREVERTICES);

	glutSwapBuffers();
}

// Called when the window is resized.
void callbackReshape(int width, int height)
{
	windowWidth = width;
	glViewport(0, 0, width, height);
}

// Called when a key is pressed. x, y is the current mouse position.
void callbackKeyboard(unsigned char key, int x, int y)
{
	if (key == 27) // esc
		exit(0);
	// Switch between mouse mode and keyboard mode
	if (key == 'm')
	{
		if(mouseInput)
			mouseInput = false;
		else
			mouseInput = true;
	}
}

// Called when a mouse button is pressed or released
void callbackMouse(int button, int state, int x, int y)
{
	if(button == GLUT_LEFT_BUTTON && state == GLUT_DOWN)
		mouseInput = true;
}

// Called when the mouse is moved with no buttons pressed
void callbackPassiveMotion(int x, int y)
{
	int half = windowWidth / 2;
	// Clockwise if mouse on the left half of window
	// Counterclockwise if mouse on the right half of window
	if(x < half)
		mouseSphereDelta = 12 *  (half - x) / half;
	else if(x > half)
		mouseSphereDelta = -12 *  (x - half) / half;
	else
		mouseSphereDelta = 0;
}

void collisionDetect(int tunnelIndex) {
	for (int j = 0; j < numObstacle; j++) 
		if(Obstacle.exists[tunnelIndex][j]) {
			float dist = distance(PlayerSphere.worldPosition, Obstacle.positions[tunnelIndex][j]);
			if (dist <= PlayerSphere.ScaleXYZ + Obstacle.ScaleXYZ) {
				//TODO collision
				changeObstacleNum(BEGINNINGNUMOBSTACLE);
				numObstacleIncreases = 1;

				//score
				clock_t end_time = clock() - begin_time;
				unsigned long curr_score = end_time * 1000 / CLOCKS_PER_SEC;
				if (curr_score > highscore)
					highscore = curr_score;
				if (curr_score > 200)
					printf("Score: %u\t\tHighscore: %u\n", curr_score, highscore);

				begin_time = clock();
				PlayerSphere.posTheta = PlayerSphere.StartDegrees;
				float radians = PlayerSphere.posTheta * M_PI / 180.0;
				PlayerSphere.worldPosition = vec4(cosf(radians)*PlayerSphere.MoveRadius, sinf(radians)*PlayerSphere.MoveRadius, PlayerSphere.StartPosZ, 1.0);
			}
		}
}
// Called when the timer expires
void callbackTimer(int)
{
	//speed up as time passes
	clock_t time_diff = clock() - begin_time;
	if (FORWARDSPEED < MAXSPEED)
		FORWARDSPEED = DEFAULTSPEED + time_diff/500;
	if (time_diff > 5000 * numObstacleIncreases && numObstacle < MAXNUMOBSTACLE) {
		changeObstacleNum(numObstacle+1);
		numObstacleIncreases++;
		//printf("Number of Obstacles: %d\n", numObstacle);
	}

	glutTimerFunc(1000/FPS, callbackTimer, 0);
	for(int i=0; i < NUMTUNNELSECTIONS; i++)
	{
		Tunnel.tunnelSectionZPositions[i] += (float)FORWARDSPEED/FPS;
		Tunnel.objToworldMs[i] = Translate(0, 0, Tunnel.tunnelSectionZPositions[i]) *
			Tunnel.scaleM;
		for (int j = 0; j < numObstacle; j++) {
			if (Obstacle.exists[i][j])
				Obstacle.positions[i][j].z += (float)FORWARDSPEED/FPS;
		}
	}

	// Move sphere using mouse mode
	if(mouseInput)
	{
		PlayerSphere.posTheta -= mouseSphereDelta;
		if(PlayerSphere.posTheta < 0)
		{
			PlayerSphere.posTheta += 360;
		}
		float radians = PlayerSphere.posTheta * M_PI / 180.0;
		PlayerSphere.worldPosition.x = cosf(radians) * SPHEREMOVERADIUS;
		PlayerSphere.worldPosition.y = sinf(radians) * SPHEREMOVERADIUS;
		glUseProgram(PlayerSphere.program);
		glUniformMatrix4fv(
			PlayerSphere.index_objToworldM,
			1,
			GL_TRUE,
			Translate(
				PlayerSphere.worldPosition.x,
				PlayerSphere.worldPosition.y,
				PlayerSphere.worldPosition.z) *
			PlayerSphere.scaleM);

		glUseProgram(Tunnel.program);
		glUniform4fv(
			Tunnel.index_lightPosition,
			1,
			PlayerSphere.worldPosition);
		glUseProgram(0);
	}


	// Check if the sphere has moved past the first tunnel section
	if(Tunnel.tunnelSectionZPositions[Tunnel.firstTunnelSection] > (CYLINDERSCALEZ*2))
	{
		// Move the first section to the back
		Tunnel.tunnelSectionZPositions[Tunnel.firstTunnelSection] -=
			NUMTUNNELSECTIONS*(CYLINDERSCALEZ*2);
		Tunnel.objToworldMs[Tunnel.firstTunnelSection] =
			Translate(0, 0, Tunnel.tunnelSectionZPositions[Tunnel.firstTunnelSection]) *
			Tunnel.scaleM;

		//Obstacle
		for (int j = 0; j < numObstacle; j++) {
			if (rand() % OBSTACLEPROB) {
				Obstacle.exists[Tunnel.firstTunnelSection][j] = true;
				int theta = rand() % 18;
				float radians = (theta*20) * M_PI / 180.0;
				Obstacle.positions[Tunnel.firstTunnelSection][j] = vec4(
					cosf(radians) * Obstacle.MoveRadius,
					sinf(radians) * Obstacle.MoveRadius,	
					Tunnel.tunnelSectionZPositions[Tunnel.firstTunnelSection],
					1.0);
			} else {
				Obstacle.exists[Tunnel.firstTunnelSection][j] = false;
			}
		}

		Tunnel.firstTunnelSection = (Tunnel.firstTunnelSection+1) % NUMTUNNELSECTIONS;
	}

	//check collision for first two tunnels, only possible places player sphere can exist
	for (int i=0; i < NUMTUNNELSECTIONS; i++) { 
		collisionDetect(i);
	}

	glutPostRedisplay();
}

void specialCallback(int key, int x, int y)
{
	mouseInput = false;
	switch(key)
	{
	case GLUT_KEY_LEFT:
		{
			PlayerSphere.posTheta -= PlayerSphere.MoveDelta;
		if(PlayerSphere.posTheta < 0)
		{
			PlayerSphere.posTheta += 360;
		}
		float radians = PlayerSphere.posTheta * M_PI / 180.0;
		PlayerSphere.worldPosition.x = cosf(radians) * PlayerSphere.MoveRadius;
		PlayerSphere.worldPosition.y = sinf(radians) * PlayerSphere.MoveRadius;
		glUseProgram(PlayerSphere.program);
		glUniform1i(glGetUniformLocation(PlayerSphere.program, "texture"), 0 );
		glUniformMatrix4fv(
			PlayerSphere.index_objToworldM,
			1,
			GL_TRUE,
			Translate(
				PlayerSphere.worldPosition.x,
				PlayerSphere.worldPosition.y,
				PlayerSphere.worldPosition.z) *
			PlayerSphere.scaleM);

		glUseProgram(0);
		break;
		}
	case GLUT_KEY_RIGHT:
		{
		PlayerSphere.posTheta += PlayerSphere.MoveDelta;
		if(PlayerSphere.posTheta > 360)
		{
			PlayerSphere.posTheta -= 360;
		}
		float radians = PlayerSphere.posTheta * M_PI / 180.0;
		PlayerSphere.worldPosition.x = cosf(radians) * PlayerSphere.MoveRadius;
		PlayerSphere.worldPosition.y = sinf(radians) * PlayerSphere.MoveRadius;
		glUseProgram(PlayerSphere.program);
		glUniform1i(glGetUniformLocation(PlayerSphere.program, "texture"), 0 );
		glUniformMatrix4fv(
			PlayerSphere.index_objToworldM,
			1,
			GL_TRUE,
			Translate(
				PlayerSphere.worldPosition.x,
				PlayerSphere.worldPosition.y,
				PlayerSphere.worldPosition.z) *
			PlayerSphere.scaleM);

		glUseProgram(0);
		break;
		}
	}
}

void initCallbacks()
{
	glutDisplayFunc(callbackDisplay);
	glutReshapeFunc(callbackReshape);
	glutKeyboardFunc(callbackKeyboard);
	glutMouseFunc(callbackMouse);
	glutPassiveMotionFunc(callbackPassiveMotion);
	glutSpecialFunc(specialCallback);
	glutTimerFunc(1000/30, callbackTimer, 0);
}

int main(int argc, char** argv)
{
	initGlut(argc, argv);
	initCallbacks();
	glewExperimental = GL_TRUE;
	glewInit();
	init();
	glutMainLoop();
	return 0;
}

float distance(vec4 v1, vec4 v2) {
	float xd = v2.x - v1.x;
	float yd = v2.y - v1.y;
	float zd = v2.z - v1.z;
	return sqrt(xd*xd+yd*yd+zd*zd);
}

vec4 unit( const vec4& p)
{
	float len = p.x*p.x + p.y * p.y + p.z * p.z;

	vec4 t;
	if(len > DivideByZeroTolerance) {
		t = p / sqrt(len);
		t.w = 1.0;
	}

	return t;
}

int Index = 0;
void triangle( const vec4& a, const vec4& b, const vec4& c, sphere &Sphere )
{
	Sphere.normals[Index] = vec3(a.x, a.y, a.z);
	Sphere.points[Index] = a;
	vec4 unitA = unit(a);
	Sphere.texCoords[Index] = vec2(asin(unitA.x)/M_PI + .5, acos(unitA.y)/M_PI + .5);
	Index++;
	Sphere.normals[Index] = vec3(b.x, b.y, b.z);
	Sphere.points[Index] = b;
	vec4 unitB = unit(b);
	Sphere.texCoords[Index] = vec2(asin(unitB.x)/M_PI + .5, acos(unitB.y)/M_PI + .5);
	Index++;
	Sphere.normals[Index] = vec3(c.x, c.y, c.z);
	Sphere.points[Index] = c;
	vec4 unitC = unit(c);
	Sphere.texCoords[Index] = vec2(asin(unitC.x)/M_PI + .5, acos(unitC.y)/M_PI + .5); 
	Index++;
}

void divide_triangle( const vec4& a, const vec4& b, const vec4& c, int count, sphere &Sphere)
{
	if(count > 0)
	{
		vec4 v1 = unit(a + b);
		vec4 v2 = unit(a + c);
		vec4 v3 = unit(b + c);
		divide_triangle(a, v1, v2, count-1, Sphere);
		divide_triangle(c, v2, v3, count-1, Sphere);
		divide_triangle(b, v3, v1, count-1, Sphere);
		divide_triangle(v1, v3, v2, count-1, Sphere);
	}
	else
	{
		triangle(a, b, c, Sphere);
		//*ppoints += 3;
		//*pnormals += 3;
	}
}

void tetrahedron(int count, sphere &Sphere)
{
	vec4 v[4] = {
		vec4(0.0, 0.0, 1.0, 1.0),
		vec4(0.0, 0.942809, -0.333333, 1.0),
		vec4(-0.816497, -0.471405, -0.333333, 1.0),
		vec4(0.816497, -0.471405, -0.333333, 1.0)
	};

	divide_triangle(v[0], v[1], v[2], count, Sphere);
	divide_triangle(v[3], v[2], v[1], count, Sphere);
	divide_triangle(v[0], v[3], v[1], count, Sphere);
	divide_triangle(v[0], v[2], v[3], count, Sphere);
}

void init_sphere(struct sphere &Sphere)
{
	//vec4 *sphere_vertices = (vec4*)malloc(sizeof(vec4)*NUMSPHEREVERTICES);
	//vec3 *sphere_normals = (vec3*)malloc(sizeof(vec3)*NUMSPHEREVERTICES);
	Index = 0;
	tetrahedron(NUMSPHERETIMESTODIVIDE, Sphere);

	Sphere.program = init_shaders(SPHEREVSHADER, SPHEREFSHADER);

	glGenVertexArrays(1, &Sphere.varray);
	glGenBuffers(1, &Sphere.buffer);
	glBindBuffer(GL_ARRAY_BUFFER, Sphere.buffer);
	glBufferData(
		GL_ARRAY_BUFFER,
		sizeof(Sphere.points) + sizeof(Sphere.normals) + sizeof(Sphere.texCoords),
		0,
		GL_STATIC_DRAW);
	glBufferSubData(
		GL_ARRAY_BUFFER,
		0,
		sizeof(Sphere.points),
		Sphere.points);
	glBufferSubData(
		GL_ARRAY_BUFFER,
		sizeof(Sphere.points),
		sizeof(Sphere.normals),
		Sphere.normals);
	glBufferSubData(
		GL_ARRAY_BUFFER,
		sizeof(Sphere.points) + sizeof(Sphere.normals),
		sizeof(Sphere.texCoords),
		Sphere.texCoords);

	glBindVertexArray(Sphere.varray);
	
	GLuint index_vPosition = glGetAttribLocation(Sphere.program, "vPosition");
	glEnableVertexAttribArray(index_vPosition);
	glVertexAttribPointer(index_vPosition, 4, GL_FLOAT, GL_FALSE, 0, BUFFER_OFFSET(0));
	GLuint index_vNormal = glGetAttribLocation(Sphere.program, "vNormal");
	glEnableVertexAttribArray(index_vNormal);
	glVertexAttribPointer(
		index_vNormal,
		3,
		GL_FLOAT,
		GL_FALSE,
		0,
		BUFFER_OFFSET(sizeof(Sphere.points)));
	GLuint index_vTexCoord = glGetAttribLocation(Sphere.program, "vTexCoord");
	glEnableVertexAttribArray(index_vTexCoord);
	glVertexAttribPointer(
		index_vTexCoord,
		2,
		GL_FLOAT,
		GL_FALSE,
		0,
		BUFFER_OFFSET(sizeof(Sphere.points) + sizeof(Sphere.normals)));

	GLuint vTexCoord = glGetAttribLocation( Sphere.program, "vTexCoord" );

	// Uniform initialization
	glUseProgram(Sphere.program);
	GLuint index_perspectiveM = glGetUniformLocation(Sphere.program, "perspectiveM");
	glUniformMatrix4fv(index_perspectiveM, 1, GL_TRUE, perspectiveM);

	Sphere.ambientProduct =
		vec4(Sphere.Ambient, Sphere.Ambient, Sphere.Ambient, 1.0);
	GLuint index_ambientProduct = glGetUniformLocation(Sphere.program, "ambientProduct");
	glUniform4fv(index_ambientProduct, 1, Sphere.ambientProduct);

	Sphere.diffuseProduct =
		vec4(Sphere.Diffuse, Sphere.Diffuse, Sphere.Diffuse, 1.0);
	GLuint index_diffuseProduct = glGetUniformLocation(Sphere.program, "diffuseProduct");
	glUniform4fv(index_diffuseProduct, 1, Sphere.diffuseProduct);

	Sphere.specularProduct = 
		vec4(Sphere.Specular, Sphere.Specular, Sphere.Specular, 1.0);
	GLuint index_specularProduct = glGetUniformLocation(Sphere.program, "specularProduct");
	glUniform4fv(index_specularProduct, 1, Sphere.specularProduct);

	GLuint index_shininess = glGetUniformLocation(Sphere.program, "shininess");
	glUniform1f(index_shininess, Sphere.Shininess);

	GLuint index_worldTocamM = glGetUniformLocation(Sphere.program, "worldTocamM");
	glUniformMatrix4fv(index_worldTocamM, 1, GL_TRUE, worldTocamM);

	Sphere.scaleM = Scale(Sphere.ScaleXYZ, Sphere.ScaleXYZ, Sphere.ScaleXYZ);

	Sphere.index_objToworldM = glGetUniformLocation(Sphere.program, "objToworldM");
	glUniformMatrix4fv(
		Sphere.index_objToworldM,
		1,
		GL_TRUE,
		Translate(Sphere.worldPosition) *
		Sphere.scaleM);

	// Load image using SOIL and generate texture

	glGenTextures( 4, textures );
	glBindTexture( GL_TEXTURE_2D, textures[0]);
	glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT );
    glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT );
    glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
    glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
	glGenerateMipmap( GL_TEXTURE_2D );

	int width, height;
	unsigned char* image = SOIL_load_image( "fire.jpg", &width, &height, 0, SOIL_LOAD_RGB );
	glTexImage2D( GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, image );

	glBindTexture( GL_TEXTURE_2D, textures[1]);
	glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT );
    glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT );
    glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
    glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
	glGenerateMipmap( GL_TEXTURE_2D );

	int width2, height2;
	unsigned char* image2 = SOIL_load_image( "water.jpg", &width2, &height2, 0, SOIL_LOAD_RGB );
	glTexImage2D( GL_TEXTURE_2D, 0, GL_RGB, width2, height2, 0, GL_RGB, GL_UNSIGNED_BYTE, image2 );

	// Cleanup
	//free(sphere_vertices);
	//free(sphere_normals);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);
}

void cylinder(int segments, vec4 *points, vec3 *normals, vec2 *texCoords)
{
	double degrees = 0;
	double degrees_inc = 360.0 / segments;
	int vertex = 0;
	for(int i=0; i < segments; i++)
	{
		float x1 = cosf(degrees * M_PI / 180.0);
		float y1 = sinf(degrees * M_PI / 180.0);
		float x2 = cosf((degrees+degrees_inc) * M_PI / 180.0);
		float y2 = sinf((degrees+degrees_inc) * M_PI / 180.0);
		// Triangle 1
		points[vertex] = vec4(x1, y1, -1.0, 1.0);
		normals[vertex] = vec3(-x1, -y1, 0);
		texCoords[vertex] = vec2(x1, 0.0);
		vertex++;
		points[vertex] = vec4(x1, y1, 1.0, 1.0);
		normals[vertex] = vec3(-x1, -y1, 0);
		texCoords[vertex] = vec2(x1, 1.0);
		vertex++;
		points[vertex] = vec4(x2, y2, -1.0, 1.0);
		normals[vertex] = vec3(-x2, -y2, 0);
		texCoords[vertex] = vec2(x2, 0.0);
		vertex++;
		//Triangle 2
		points[vertex] = vec4(x2, y2, -1.0, 1.0);
		normals[vertex] = vec3(-x2, -y2, 0);
		texCoords[vertex] = vec2(x2, 0.0);
		vertex++;
		points[vertex] = vec4(x1, y1, 1.0, 1.0);
		normals[vertex] = vec3(-x1, -y1, 0);
		texCoords[vertex] = vec2(x1, 1.0);
		vertex++;
		points[vertex] = vec4(x2, y2, 1.0, 1.0);
		normals[vertex] = vec3(-x2, -y2, 0);
		texCoords[vertex] = vec2(x2, 1.0);
		vertex++;

		degrees += degrees_inc;
	}
}

void init_cylinder(struct cylinder &Cylinder)
{
	vec4 *cylinder_vertices = (vec4*)malloc(sizeof(vec4)*NUMCYLINDERVERTICES);
	vec3 *cylinder_normals = (vec3*)malloc(sizeof(vec3)*NUMCYLINDERVERTICES);
	vec2 *cylinder_texCoords = (vec2*)malloc(sizeof(vec2)*NUMCYLINDERVERTICES);

	cylinder(NUMCYLINDERSEGMENTS, cylinder_vertices, cylinder_normals, cylinder_texCoords);

	Cylinder.program = init_shaders(CYLINDERVSHADER, CYLINDERFSHADER);

	glGenVertexArrays(1, &Cylinder.varray);
	glGenBuffers(1, &Cylinder.buffer);
	glBindBuffer(GL_ARRAY_BUFFER, Cylinder.buffer);
	glBufferData(
		GL_ARRAY_BUFFER,
		sizeof(vec4)*NUMCYLINDERVERTICES + sizeof(vec3)*NUMCYLINDERVERTICES + sizeof(vec2)*NUMCYLINDERVERTICES,
		0,
		GL_STATIC_DRAW);
	glBufferSubData(
		GL_ARRAY_BUFFER,
		0,
		sizeof(vec4)*NUMCYLINDERVERTICES,
		cylinder_vertices);
	glBufferSubData(
		GL_ARRAY_BUFFER,
		sizeof(vec4)*NUMCYLINDERVERTICES,
		sizeof(vec3)*NUMCYLINDERVERTICES,
		cylinder_normals);
	glBufferSubData(
		GL_ARRAY_BUFFER,
		sizeof(vec4)*NUMCYLINDERVERTICES + sizeof(vec3)*NUMCYLINDERVERTICES,
		sizeof(vec2)*NUMCYLINDERVERTICES,
		cylinder_texCoords);
	

	glBindVertexArray(Cylinder.varray);
	GLuint index_vPosition = glGetAttribLocation(Cylinder.program, "vPosition");
	glEnableVertexAttribArray(index_vPosition);
	glVertexAttribPointer(index_vPosition, 4, GL_FLOAT, GL_FALSE, 0, 0);
	GLuint index_vNormal = glGetAttribLocation(Cylinder.program, "vNormal");
	glEnableVertexAttribArray(index_vNormal);
	glVertexAttribPointer(
		index_vNormal,
		3,
		GL_FLOAT,
		GL_FALSE,
		0,
		(void*)(sizeof(vec4)*NUMCYLINDERVERTICES));
	GLuint index_vTexCoord = glGetAttribLocation(Cylinder.program, "vTexCoord");
	glEnableVertexAttribArray(index_vTexCoord);
	glVertexAttribPointer(
		index_vTexCoord,
		2,
		GL_FLOAT,
		GL_FALSE,
		0,
		(void*)(sizeof(vec4)*NUMCYLINDERVERTICES + sizeof(vec3)*NUMCYLINDERVERTICES));

	GLuint vTexCoord = glGetAttribLocation( Cylinder.program, "vTexCoord" );

	// Uniform initialization
	glUseProgram(Cylinder.program);

	GLuint index_perspectiveM = glGetUniformLocation(Cylinder.program, "perspectiveM");
	glUniformMatrix4fv(index_perspectiveM, 1, GL_TRUE, perspectiveM);

	Cylinder.firstTunnelSection = 0;
	Cylinder.scaleM = Scale(CYLINDERSCALEXY, CYLINDERSCALEXY, CYLINDERSCALEZ);
	for(int i=0; i < NUMTUNNELSECTIONS; i++)
	{
		Cylinder.tunnelSectionZPositions[i] = -i*(CYLINDERSCALEZ*2);
		Cylinder.objToworldMs[i] = Translate(0, 0, -i*(CYLINDERSCALEZ*2)) *
			Cylinder.scaleM;

		//obstacle init
		for (int j = 0; j < numObstacle; j++) {
			if (rand() % OBSTACLEPROB) {
				Obstacle.exists[i][j] = true;
				int theta = rand() % 18;
				float radians = (theta * 20) * M_PI / 180.0;
				Obstacle.positions[i][j] = vec4(
					cosf(radians) * Obstacle.MoveRadius,
					sinf(radians) * Obstacle.MoveRadius,	
					Cylinder.tunnelSectionZPositions[i],
					1.0);
			} else {
				Obstacle.exists[i][j] = false;
			}
		}
	}
	Cylinder.index_objToworldM = glGetUniformLocation(Cylinder.program, "objToworldM");

	GLuint index_worldTocamM = glGetUniformLocation(Cylinder.program, "worldTocamM");
	glUniformMatrix4fv(index_worldTocamM, 1, GL_TRUE, worldTocamM);

	// Shading uniforms
	Cylinder.index_lightPosition = glGetUniformLocation(Cylinder.program, "lightPosition");
	float radians = PlayerSphere.StartDegrees * M_PI / 180.0;
	glUniform4fv(
		Cylinder.index_lightPosition,
		1,
		vec4(
			cosf(radians)*PlayerSphere.MoveRadius,
			sinf(radians)*PlayerSphere.MoveRadius,
			PlayerSphere.StartPosZ,
			1.0));

	Cylinder.ambientProduct =
		vec4(TUNNELAMBIENT, TUNNELAMBIENT, TUNNELAMBIENT, 1.0) *
		vec4(PlayerSphere.Ambient, PlayerSphere.Ambient, PlayerSphere.Ambient, 1.0);
	GLuint index_ambientProduct = glGetUniformLocation(Cylinder.program, "ambientProduct");
	glUniform4fv(index_ambientProduct, 1, Cylinder.ambientProduct);

	Cylinder.diffuseProduct =
		vec4(TUNNELDIFFUSE, TUNNELDIFFUSE, TUNNELDIFFUSE, 1.0) *
		vec4(PlayerSphere.Diffuse, PlayerSphere.Diffuse, PlayerSphere.Diffuse, 1.0);
	GLuint index_diffuseProduct = glGetUniformLocation(Cylinder.program, "diffuseProduct");
	glUniform4fv(index_diffuseProduct, 1, Cylinder.diffuseProduct);

	Cylinder.specularProduct =
		vec4(TUNNELSPECULAR, TUNNELSPECULAR, TUNNELSPECULAR, 1.0) *
		vec4(PlayerSphere.Specular, PlayerSphere.Specular, PlayerSphere.Specular, 1.0);
	GLuint index_specularProduct = glGetUniformLocation(Cylinder.program, "specularProduct");
	glUniform4fv(index_specularProduct, 1, Cylinder.specularProduct);

	Cylinder.shininess = TUNNELSHININESS;
	GLuint index_shininess = glGetUniformLocation(Cylinder.program, "shininess");
	glUniform1f(index_shininess, Cylinder.shininess);

	// Load image using SOIL and generate texture
	glBindTexture( GL_TEXTURE_2D, textures[2]);
	glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT );
    glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT );
    glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
    glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
	glGenerateMipmap( GL_TEXTURE_2D );

	int width, height;
	unsigned char* image = SOIL_load_image( "crate.jpg", &width, &height, 0, SOIL_LOAD_RGB );
	glTexImage2D( GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, image );
	

	// Cleanup
	free(cylinder_vertices);
	free(cylinder_normals);
	free(cylinder_texCoords);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);
	glUseProgram(0);
}

GLuint init_shaders(const char *vshadername, const char *fshadername)
{
	/* Compile shaders */
	GLuint program;
	GLuint vshader;
	GLuint fshader;
	vshader = glCreateShader(GL_VERTEX_SHADER);
	fshader = glCreateShader(GL_FRAGMENT_SHADER);

	FILE *vshadersrc = fopen(vshadername, "rb");
	FILE *fshadersrc = fopen(fshadername, "rb");

	// get size of each shader file
	fseek(vshadersrc, 0, SEEK_END);
	fseek(fshadersrc, 0, SEEK_END);
	size_t vshadersrc_size = ftell(vshadersrc);
	size_t fshadersrc_size = ftell(fshadersrc);
	rewind(vshadersrc);
	rewind(fshadersrc);

	char * vshader_data = (char*)malloc(vshadersrc_size+1);
	char * fshader_data = (char*)malloc(fshadersrc_size+1);
	fread(vshader_data, 1, vshadersrc_size, vshadersrc);
	fread(fshader_data, 1, fshadersrc_size, fshadersrc);

	vshader_data[vshadersrc_size] = 0;
	fshader_data[fshadersrc_size] = 0;
	fclose(vshadersrc);
	fclose(fshadersrc);

	glShaderSource(vshader, 1, (const GLchar**)&vshader_data, 0);
	glCompileShader(vshader);
	GLint result;
	glGetShaderiv(vshader, GL_COMPILE_STATUS, &result);
	GLsizei len, slen = 256;
	GLchar log[256];
	if(result != 1)
		glGetShaderInfoLog(	vshader,
 		slen,
 		&len,
 		log);

	glShaderSource(fshader, 1, (const GLchar**)&fshader_data, 0);
	glCompileShader(fshader);
	glGetShaderiv(fshader, GL_COMPILE_STATUS, &result);
	if(result != 1)
		glGetShaderInfoLog(	fshader,
 		slen,
 		&len,
 		log);

	program = glCreateProgram();
	glAttachShader(program, vshader);
	glAttachShader(program, fshader);
	glLinkProgram(program);

	free(vshader_data);
	free(fshader_data);

	return program;
}