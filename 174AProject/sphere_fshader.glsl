#version 150

in vec3 fN;
in vec3 fL;
in vec3 fE;
in vec2 texCoord;

uniform vec4 ambientProduct, diffuseProduct, specularProduct;
uniform float shininess;
uniform sampler2D texture;

void main()
{
	vec3 N = normalize(fN);
	vec3 E = normalize(fE);
	vec3 L = normalize(fL);

	vec3 H = normalize( L + E );

	vec4 ambient = ambientProduct;

	float Kd = max(dot(L, N), 0.0);
	float dist = length(fL);
	vec4 diffuse = Kd * diffuseProduct / (dist);

	float Ks = pow(max(dot(N, H), 0.0), shininess);
	Ks /= (dist/100);
	vec4 specular = Ks*specularProduct;

	if( dot(L, N) < 0.0 ) {
		specular = vec4(0.0, 0.0, 0.0, 1.0);
	}

	vec4 color = diffuse + specular + ambient;
	color.a = 1.0;

	gl_FragColor = color * texture2D( texture, texCoord );
}