#include "gpu_algorithm.h"

namespace cgv {
namespace gpgpu {

void gpu_algorithm::create_buffer(GLuint& buffer, size_t size, GLenum usage) {

	glGenBuffers(1, &buffer);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, buffer);
	glBufferData(GL_SHADER_STORAGE_BUFFER, size, (void*)0, usage);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

void gpu_algorithm::delete_buffer(GLuint& buffer) {

	if(buffer != 0) {
		glDeleteBuffers(1, &buffer);
		buffer = 0;
	}
}

void gpu_algorithm::begin_time_query() {

	time_query = 0;
	glGenQueries(1, &time_query);

	glBeginQuery(GL_TIME_ELAPSED, time_query);
}

float gpu_algorithm::end_time_query() {

	glEndQuery(GL_TIME_ELAPSED);

	GLint done = false;
	while(!done) {
		glGetQueryObjectiv(time_query, GL_QUERY_RESULT_AVAILABLE, &done);
	}
	GLuint64 elapsed_time = 0;
	glGetQueryObjectui64v(time_query, GL_QUERY_RESULT, &elapsed_time);

	return elapsed_time / 1000000.0f;
}

}
}
