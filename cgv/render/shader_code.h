#pragma once

#include <cgv/render/context.h>

#include "lib_begin.h"

namespace cgv {
	namespace render {

/** a globally unique shader config is registered by default when the cgv library
    is used. Currently it has one member only defining the search path for shader
	 files. The shader path is initialized to the environment variable CGV_SHADER_PATH
	 or empty otherwise. It can also be set with a command line argument of the form
	 
	 type(shader_config):shader_path='@(INPUT_PATH)'

	 or in a config file as

	 type(shader_config):shader_path='D:/my_shaders'

	 To set the shader path at runtime, query the shader_config with the
	 get_shader_config() function.
*/

	/// typedef for shader define map data structure
	typedef std::map<std::string, std::string> shader_define_map;

struct CGV_API shader_config : public cgv::base::base
{
	/// the path used to find shaders with the cgv::utils::file::find_in_paths function
	std::string shader_path;
	/// whether to keep track of file names
	bool trace_file_names;
	/// whether to output full paths of read shaders
	bool show_file_paths;
	/// mapping of shader index to file name
	std::vector<std::string> shader_file_names;
	/// mapping of shader index to inserted files name
	std::vector<std::string> inserted_shader_file_names;
	/// construct config without file name tracing
	shader_config();
	/// return "shader_config"
	std::string get_type_name() const;
	/// reflect the shader_path member
	bool self_reflect(cgv::reflect::reflection_handler& srh);
};

/// type of ref counted pointer to shader configuration
typedef cgv::data::ref_ptr<shader_config> shader_config_ptr;

/// return a pointer to the current shader configuration
extern CGV_API shader_config_ptr get_shader_config();

/** a shader code object holds a code fragment of a geometry
    vertex or fragment shader and can be added to a shader 
	 program. */
class CGV_API shader_code : public render_component
{
public:
	template<typename T, typename std::enable_if<!std::is_enum<T>::value, bool>::type = true>
	static void set_define(shader_define_map& defines, const std::string& name, const T& value, const T& default_value) {
		if(value != default_value)
			defines[name] = std::to_string(value);
		else
			defines.erase(name);
	}
	
	template<typename T, typename std::enable_if<std::is_enum<T>::value, bool>::type = true>
	static void set_define(shader_define_map& defines, const std::string& name, const T& value, const T& default_value) {
		if(value != default_value)
			defines[name] = std::to_string((unsigned)value);
		else
			defines.erase(name);
	}

	static void set_define(shader_define_map& defines, const std::string& name, const std::string& value, const std::string& default_value) {
		if(value != default_value)
			defines[name] = value;
		else
			defines.erase(name);
	}

	static void set_define(shader_define_map& defines, const std::string& name, const bool& value, const bool& default_value) {
		if(value != default_value)
			defines[name] = value ? "1" : "0";
		else
			defines.erase(name);
	}

protected:
	/// store the shader type
	ShaderType st;

public:
	///create shader a shader code object
	shader_code();
	/// calls the destruct method
	~shader_code();
	/// decode a string if it is base64 encoded
	static void decode_if_base64(std::string& content);
	/** Check if file exists. If not, check if a resource file of this
	    file_name has been registered. If not search it recursively in the 
	    shader_path of the shader_config that can be accessed with the
		 function get_shader_config(). This path is initialized to the 
		 environment variable CGV_SHADER_PATH or empty if that is not 
		 defined. */
	static std::string find_file(const std::string& file_name);
	/** format given last error in a way that developer environments can locate errors in the source file */
	static std::string get_last_error(const std::string& file_name, const std::string& last_error);
	/// read shader code from file and return string with content or empty string if read failed
	static std::string read_code_file(const std::string &file_name, std::string* _last_error = 0);
	/** detect the shader type from the extension of the given
		 file_name, i.e.		 
		 - glvs ... ST_VERTEX
		 - glgs ... ST_GEOMETRY
		 - glfs ... ST_FRAGMENT
		 - glcs ... ST_COMPUTE
	*/
	static ShaderType detect_shader_type(const std::string& file_name);
	/// destruct shader code
	void destruct(const context& ctx);
	/** read shader code from file that is searched for with find_file.
	    If the shader type defaults to ST_DETECT, the detect_shader_type()
		 method is applied to the file name.*/
	bool read_code(const context& ctx, const std::string &file_name, ShaderType st = ST_DETECT, const shader_define_map& defines = shader_define_map());
	/// set shader code from string
	bool set_code(const context& ctx, const std::string &source, ShaderType st);
	/// set shader code defines
	void set_defines(std::string& source, const shader_define_map& defines);
	/// set shader code vertex attribute locations (a hotfix for AMD driver behaviour on vertex shaders)
	void set_vertex_attrib_locations(std::string& source);
	/// return the shader type of this code
	ShaderType get_shader_type() const;
	///compile attached source; returns true if successful
	bool compile(const context& ctx);
	/** read shader code with read_code and compile. If show_error is true
	    print error messages formated with the get_last_error method in case
		 an error arose. */
	bool read_and_compile(const context& ctx, const std::string &file_name, ShaderType st = ST_DETECT, bool show_error = true, const shader_define_map& defines = shader_define_map());
	/// return whether shader has been compiled successfully
	bool is_compiled() const;
};


	}
}

#include <cgv/config/lib_end.h>
