#version 330 core

// input data : sent from main program
layout (location = 0) in vec3 vertexPosition;
layout (location = 2) in vec2 vertexTexCoord;

uniform mat4 MVP;
uniform vec3 objectPosition;
uniform vec3 playerPosition;
uniform float playerAngle;
uniform float level;
uniform float isPortal;

// output data : used by fragment shader
out vec2 fragTexCoord;
out vec3 objectPositionout;
out vec3 playerPositionout;
out float playerAngleout;
out float levelout;
out float isPortalout;
void main ()
{
    vec4 v = vec4(vertexPosition, 1); // Transform an homogeneous 4D vector

    // The texture coord of each vertex will be interpolated
    // to produce the color of each fragment
    fragTexCoord = vertexTexCoord;

    // Output position of the vertex, in clip space : MVP * position
    gl_Position = MVP * v;

    objectPositionout = objectPosition + vertexPosition;
    playerPositionout = playerPosition;
    playerAngleout = playerAngle;
    levelout = level;
    isPortalout = isPortal;
}
