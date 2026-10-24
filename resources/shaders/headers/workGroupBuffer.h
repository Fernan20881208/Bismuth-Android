/*
    This contains the info for a single invocation of the
    object culling compute shader. It tells the shader
    which input sprites to process. The input sprites
    can be found inside the input sprite buffer.
*/
struct WorkGroup {
    uint inputSpriteStart;
    uint inputSpriteEnd;
    // This tells the compute shader where to output the indicies in the culled index buffer
    uint culledIndexBufferOutputIndex;
};

STORAGE_BUFFER(WORK_GROUP_BUFFER_BINDING) WorkGroupBuffer {
    WorkGroup u_workGroups[CPP_ONLY(0)];
};