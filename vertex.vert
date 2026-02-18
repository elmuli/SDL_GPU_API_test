#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;

layout(set = 0, binding = 1) uniform CameraUBO {
    mat4 model;
    mat4 view;
    mat4 proj;
} camera;

layout(location = 0) out vec3 fragNormal;
layout(location = 1) out vec2 fragTexCoord;
layout(location = 2) out vec4 debugColor;

void main()
{
    mat4 mvp = camera.proj * camera.view * camera.model;

    gl_Position = mvp * vec4(inPosition, 1.0);

    debugColor = vec4(
        camera.model[3][0] * 0.5 + 0.5,
        camera.model[3][1] * 0.5 + 0.5,
        camera.model[3][2] * 0.5 + 0.5,
        1.0
    );

    fragNormal = inNormal;
    fragTexCoord = inTexCoord;
}
