
for file in `ls *vert`; do
if [ ${file} -nt ${file}.spv ]; then
/c/VulkanSDK/1.3.261.1/Bin/glslc.exe $file -o ${file}.spv
/c/VulkanSDK/1.3.261.1/Bin/spirv-cross.exe ${file}.spv --reflect --output ${file}.json
fi
done
for file in `ls *frag`; do
if [ ${file} -nt ${file}.spv ]; then
/c/VulkanSDK/1.3.261.1/Bin/glslc.exe $file -o ${file}.spv
/c/VulkanSDK/1.3.261.1/Bin/spirv-cross.exe ${file}.spv --reflect --output ${file}.json
fi
done
/C/Repos/VKEngine/x64/Debug/ShaderToHeader.exe .