#define VKLIGHT_POINT 0
#define VKLIGHT_DIR 1
#define VKLIGHT_SPOT 2

struct LightGeneric {
  vec4 Diffuse;
  vec4 Specular;
  vec4 Ambient;
  vec4     Position;
  vec4     Direction;
  float         Range;
  float         Falloff;
  float         Attenuation0;
  float         Attenuation1;
  float         Attenuation2;
  float         Theta;
  float         Phi;
  uint  Type;
};

struct DX8Material
{
	vec4 Diffuse;
	vec4 Ambient;
	vec4 Specular;
	vec4 Emissive;
	float Shininess;
};

struct LightCollection
{
	LightGeneric lights[4];
	uint lightCount;
};
layout(location = 9) in vec3 fragPos;
layout(location = 10) in vec3 viewPos;

vec3 CalcDirLight(LightGeneric light, DX8Material material, vec3 normal, vec3 fragPos, vec3 viewPos,
 vec3 inDiff, vec3 inAmb, vec3 inSpec)
{
    vec3 lightDir = normalize(-light.Direction.xyz);
    // diffuse shading
    float diff = max(dot(normal, lightDir), 0.0);
    // specular shading
	vec3 viewDir = normalize(viewPos - fragPos);
    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = (clamp(dot(viewDir, reflectDir), 0.0, 1.0) * material.Shininess);
    // combine results
    vec3 ambient  = light.Ambient.rgb * material.Ambient.rgb * inAmb;
    vec3 diffuse  = light.Diffuse.rgb * material.Diffuse.rgb  * diff * inDiff;
    vec3 specular = light.Specular.rgb * material.Specular.rgb * spec * inSpec;
	return (ambient + diffuse + specular);
}  
vec3 CalcPointLight(LightGeneric light, DX8Material material, vec3 normal, vec3 fragPos, vec3 viewPos,
 vec3 inDiff, vec3 inAmb, vec3 inSpec)
{
    vec3 lightDir = normalize(light.Position.xyz - fragPos);
    // diffuse shading
    float diff = max(dot(normal, lightDir), 0.0);
    // specular shading
	vec3 viewDir = normalize(viewPos - fragPos);
    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = (clamp(dot(viewDir, reflectDir), 0.0, 1.0) * material.Shininess);
    // attenuation
    float distance    = length(light.Position.xyz - fragPos);
    float attenuation = 1.0 / (light.Attenuation0 + light.Attenuation1 * distance + 
  			     light.Attenuation2 * (distance * distance));
    // combine results
    vec3 ambient  = light.Ambient.rgb * material.Ambient.rgb * inAmb;
    vec3 diffuse  = light.Diffuse.rgb * material.Diffuse.rgb  * diff * inDiff;
    vec3 specular = light.Specular.rgb * material.Specular.rgb * spec * inSpec;
    ambient  *= attenuation;
    diffuse  *= attenuation;
    specular *= attenuation;
    return (ambient + diffuse + specular);
}
vec3 CalcSpotLight(LightGeneric light, DX8Material material, vec3 normal, vec3 fragPos, vec3 viewPos,
 vec3 inDiff, vec3 inAmb, vec3 inSpec)
{
    vec3 lightDir = normalize(light.Position.xyz - fragPos);
    // diffuse shading
    float diff = max(dot(normal, normalize(-light.Direction.xyz)), 0.0);
    // specular shading
	vec3 viewDir = normalize(viewPos - fragPos);
    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = (clamp(dot(viewDir, reflectDir), 0.0, 1.0) * material.Shininess);
    // attenuation
	float theta = dot(lightDir, light.Direction.xyz);
    float distance    = length(light.Position.xyz - fragPos);
    float attenuation = clamp(
		(theta - light.Theta) 
		/ (light.Attenuation0 + light.Attenuation1 * distance + 
  			     light.Attenuation2 * (distance * distance)),
		0, 1);    
    // combine results
    vec3 ambient  = light.Ambient.rgb * material.Ambient.rgb * inAmb;
    vec3 diffuse  = light.Diffuse.rgb * material.Diffuse.rgb  * diff * inDiff;
    vec3 specular = light.Specular.rgb * material.Specular.rgb * spec * inSpec;
    ambient  *= attenuation;
    diffuse  *= attenuation;
    specular *= attenuation;
    return (ambient + diffuse + specular);
}

vec4 CalculateLights(LightCollection col, DX8Material material, vec3 normal, vec3 fragPos, vec3 viewPos,
 vec3 inDiff, vec3 inAmb, vec3 inSpec)
{
	if (col.lightCount == 0)
		return vec4(inDiff,1.0);
	vec4 ret = vec4(0,0,0,1);
	for (int i = 0; i < col.lightCount; ++i)
	{
		switch (col.lights[i].Type)
		{
		case VKLIGHT_DIR: ret.rgb += 
			//vec3(0.25,0,0);break;
			CalcDirLight(col.lights[i], material, normal, fragPos, viewPos, inDiff, inAmb, inSpec); break;
		case VKLIGHT_POINT: ret.rgb += 
			//vec3(0,0.25,0);break;
			CalcPointLight(col.lights[i], material, normal, fragPos, viewPos, inDiff, inAmb, inSpec); break;
		case VKLIGHT_SPOT: ret.rgb += 
			//vec3(0,0,0.25);break;
			CalcSpotLight(col.lights[i], material, normal, fragPos, viewPos, inDiff, inAmb, inSpec); break;
		}
	}
	return ret;
}
