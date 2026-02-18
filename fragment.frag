#version 450

layout(set = 0, binding = 0) uniform sampler2D Sampler;

layout(location = 0) in vec3 fragNormal;
layout(location = 1) in vec2 fragTexCoord;
layout(location = 2) in vec4 debugColor;

layout(location = 0) out vec4 outColor;

void main()
{
    vec3 lightDir_1 = normalize(vec3(0.5, 1.0, 3.5));
    vec3 lightDir_2 = normalize(vec3(-3.5, 1.0, 1));
    float diffuse_1 = max(dot(normalize(fragNormal), lightDir_1), 0.25);
    float diffuse_2 = max(dot(normalize(fragNormal), lightDir_2), 0.25);

    vec4 testColor1 = vec4(1.0, 0.0, 0.0, 1.0);
    vec4 testColor2 = vec4(0.0, 1.0, 0.0, 1.0);
    vec4 baseColor = texture(Sampler, fragTexCoord);
    vec4 color = testColor1 * diffuse_1 - testColor2 * diffuse_2;

    //outColor = baseColor;
    //outColor = vec4(color.x, color.y, color.z, 1.0);
    outColor = vec4(vec3(gl_FragCoord.z), 1.0);
    //outColor = vec4(fragNormal * 0.5 + 0.5, 1.0);

}

