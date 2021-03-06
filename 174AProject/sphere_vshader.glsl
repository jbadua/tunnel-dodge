#version 150

in vec4 vPosition;
in vec3 vNormal;
in vec2 vTexCoord;

out vec2 texCoord;
out vec3 fN;
out vec3 fE;
out vec3 fL;

uniform mat4 perspectiveM;
uniform mat4 objToworldM;
uniform mat4 worldTocamM;

uniform vec4 lightPosition;

void main()
{
	gl_Position = perspectiveM * worldTocamM * objToworldM * vPosition;

	fN = (objToworldM * vec4(vNormal.x,vNormal.y,vNormal.z,0.0)).xyz;
	fE = (vec4(0.0,0.0,35.0,1.0) - objToworldM*vPosition).xyz;
	fL = (lightPosition - objToworldM*vPosition).xyz;
	texCoord = vTexCoord;

	//vec3 pos = (worldTocamM * objToworldM * vPosition).xyz;
	//fL = (worldTocamM * lightPosition).xyz - pos;
	//fE = -pos;
	//fN = (worldTocamM * objToworldM * vec4(vNormal, 0.0)).xyz;
}