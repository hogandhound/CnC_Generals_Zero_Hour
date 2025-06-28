#define VKLIGHT_DIR 0
#define VKLIGHT_POINT 1
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

struct Material
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

vec3 CalcDirLight(LightGeneric light, Material material, vec3 normal, vec3 viewDir,
 vec3 inDiff, vec3 inAmb, vec3 inSpec)
{
    vec3 lightDir = normalize(-light.Direction.xyz);
    // diffuse shading
    float diff = max(dot(normal, lightDir), 0.0);
    // specular shading
    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), material.Shininess);
    // combine results
    vec3 ambient  = light.Ambient.rgb * material.Ambient.rgb * inAmb;
    vec3 diffuse  = light.Diffuse.rgb * material.Diffuse.rgb  * diff * inDiff;
    vec3 specular = light.Specular.rgb * material.Specular.rgb * spec * inSpec;
    return (ambient + diffuse + specular);
}  
vec3 CalcPointLight(LightGeneric light, Material material, vec3 normal, vec3 fragPos, vec3 viewDir,
 vec3 inDiff, vec3 inAmb, vec3 inSpec)
{
    vec3 lightDir = normalize(light.Position.xyz - fragPos);
    // diffuse shading
    float diff = max(dot(normal, lightDir), 0.0);
    // specular shading
    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), material.Shininess);
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
vec3 CalcSpotLight(LightGeneric light, Material material, vec3 normal, vec3 fragPos, vec3 viewDir,
 vec3 inDiff, vec3 inAmb, vec3 inSpec)
{
    vec3 lightDir = normalize(light.Position.xyz - fragPos);
    // diffuse shading
    float diff = max(dot(normal, lightDir), 0.0);
    // specular shading
    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), material.Shininess);
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

vec4 CalculateLights(LightCollection col, Material material, vec3 normal, vec3 fragPos, vec3 viewDir,
 vec3 inDiff, vec3 inAmb, vec3 inSpec)
{
	if (col.lightCount == 0)
		return vec4(inDiff,1.0);
	vec4 ret = vec4(0,0,0,1);
	for (int i = 0; i < col.lightCount; ++i)
	{
		switch (col.lights[i].Type)
		{
		case VKLIGHT_DIR: ret.rgb += CalcDirLight(col.lights[i], material, normal, viewDir, inDiff, inAmb, inSpec); break;
		case VKLIGHT_POINT: ret.rgb += CalcPointLight(col.lights[i], material, normal, fragPos, viewDir, inDiff, inAmb, inSpec); break;
		case VKLIGHT_SPOT: ret.rgb += CalcSpotLight(col.lights[i], material, normal, fragPos, viewDir, inDiff, inAmb, inSpec); break;
		}
	}
	return ret;
}
