#include "Uniforms.frag"
#include "Samplers.frag"
#include "Lighting.frag"
#include "Fog.frag"

varying vec3 vTexCoord;
#ifdef VERTEXCOLOR
    varying vec4 vColor;
#endif
varying vec3 vVertexLighting;
varying vec4 vScreenPos;

void main()
{
    #ifdef DIFFMAP
        vec4 diffInput = texture2D(sDiffMap, vTexCoord.xy);
        #ifdef ALPHAMASK
            if (diffInput.a < 0.5)
                discard;
        #endif
        vec3 diffColor = cMatDiffColor.rgb * diffInput.rgb;
    #else
        vec3 diffColor = cMatDiffColor.rgb;
    #endif

    #ifdef SPECMAP
        vec3 specColor = cMatSpecColor.rgb * texture2D(sSpecMap, vTexCoord.xy).g;
    #else
        vec3 specColor = cMatSpecColor.rgb;
    #endif

    // Lights are accumulated at half intensity. Bring back to full intensity now
    vec4 lightInput = 2.0 * texture2DProj(sLightBuffer, vScreenPos);
    vec3 lightSpecColor = lightInput.a * lightInput.rgb / max(GetIntensity(lightInput.rgb), 0.001);

    vec3 finalColor = (vVertexLighting + lightInput.rgb) * diffColor + lightSpecColor * specColor;
    gl_FragColor = vec4(GetFog(finalColor, vTexCoord.z), 1.0);
}