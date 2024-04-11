#include "bmp_writer.h"
#include <iostream>
#include <cgv/base/register.h>

#ifdef WIN32
#pragma warning (disable:4996)
#endif

namespace cgv {
	namespace media {
		namespace image {

/// default constructor
bmp_writer::bmp_writer()
{
	fp = 0;
}
/// overload to return the type name of this object
std::string bmp_writer::get_type_name() const
{
	return "bmp_writer";
}
/// construct a copy of the reader
abst_image_writer* bmp_writer::clone() const
{
	return new bmp_writer();
}
/// return a string containing a colon separated list of extensions that can be read with this reader
const char* bmp_writer::get_supported_extensions() const
{
	return "bmp";
}

/// check if the chosen writer supports the given component format
bool bmp_writer::is_format_supported(const component_format& cf, const std::vector<component_format>* palette_formats) const
{
	ComponentFormat _cf = cf.get_standard_component_format();
	if (!( (_cf == CF_RGB || _cf == CF_BGR || _cf == CF_L) && 
		    cf.get_component_type() == TI_UINT8))
		return false;
	if (palette_formats) {
		if (palette_formats->size() > 1)
			return false;
		const component_format& cfp = palette_formats->at(0);
		ComponentFormat _cfp = cfp.get_standard_component_format();
		if (!( (_cfp == CF_RGB || _cfp == CF_BGR) && 
			    cfp.get_component_type() == TI_UINT8))
			return false;
		if (cf.get_nr_components() != 1)
			return false;
	}
	return true;
		
}
/// return a colon separated list of supported options
std::string bmp_writer::get_options() const
{
	return "";
}
/// return a reference to the last error message
const std::string& bmp_writer::get_last_error() const
{
	return last_error;
}

/** write the data stored in the data view to a file with the file name given in the constructor. */
bool bmp_writer::open(const std::string& file_name)
{
	FILE* _fp = fopen(file_name.c_str(), "wb");
	if (!_fp) {
		last_error = "can't open file for writing: ";
		last_error += file_name;
		return false;
	}
	fp = _fp;
	return true;
}

/** write the data stored in the data view to a file with the file name given in the constructor. */
bool bmp_writer::write_image(const const_data_view& dv, const std::vector<const_data_view>* palettes, double duration)
{
	unsigned short width = (unsigned short) dv.get_format()->get_width();
	unsigned short height = (unsigned short) dv.get_format()->get_height();
	const unsigned char* data = dv.get_ptr<unsigned char>();

	ComponentFormat _cf = dv.get_format()->get_standard_component_format();
	if (_cf == CF_RGB || _cf == CF_BGR) {
		static unsigned char bmp_header[54] = {
			66,  77,  90,   0,   0,   0,   0,   0,   0,   0,  54,   0,   0,   0,  40,   0,   0,   0,   4,   1,   0,   0,   3,   0,   0,   0,   1,   0,  24,   0,   0,   0,   0,   0,  36,   0,   0,   0, 196,  14,   0,   0, 196,  14,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0 
		};
		((unsigned short*)bmp_header)[9]  = width;
		((unsigned short*)bmp_header)[11] = height;
		bool success = fwrite(bmp_header, 1, 54, fp) == 54;
		if (success) {
			unsigned int bytes_per_line = width*3;
			unsigned int line_padding   = 
				unsigned(packing_info::align(bytes_per_line,4)) - bytes_per_line;

			data += (height-1)*bytes_per_line;
			for (unsigned short y = 0; success && y < height; ++y) {
				if (_cf == CF_BGR) {
					success = fwrite(data, 1, bytes_per_line, fp) == bytes_per_line;
					data -= bytes_per_line;
				}
				else {
					for (unsigned short x = 0; x < width; ++x, data += 3) {
						unsigned char col[3] = { data[2], data[1], data[0] };
						if (!(success = fwrite(col, 1, 3, fp) == 3) )
							break;
					}
					data -= 2*bytes_per_line;
				}
				if (success)
					success = !(line_padding && fwrite(bmp_header+50, 1, line_padding, fp) != line_padding);
				else
					last_error = "bmp write error";
			}
		}
		else
			last_error = "could not write bmp header";
		return success;
	}
	else if (dv.get_format()->get_nr_components() == 1 || 
				(dv.get_format()->get_component_name(0) == "0" && 
				 palettes && palettes->size() == 1)) {
		unsigned char bmp_header[1078] = {
			  66,  77,  70,  43,   0,   0,   0,   0,   0,   0,  54,   4,   0,   0,  40,   0,   0,   0, 100,   0,   0,   0, 100,   0,   0,   0,   1,   0,   8,   0,   0,   0,   0,   0,  16,  39,   0,   0, 196,  14,   0,   0, 196,  14,   0,   0,   0,   1,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   1,   1,   1,   0,   2,   2,
			   2,   0,   3,   3,   3,   0,   4,   4,   4,   0,   5,   5,   5,   0,   6,   6,   6,   0,   7,   7,   7,   0,   8,   8,   8,   0,   9,   9,   9,   0,  10,  10,  10,   0,  11,  11,  11,   0,  12,  12,  12,   0,  13,  13,  13,   0,  14,  14,  14,   0,  15,  15,  15,   0,  16,  16,  16,   0,  17,  17,  17,   0,  18,  18,
			  18,   0,  19,  19,  19,   0,  20,  20,  20,   0,  21,  21,  21,   0,  22,  22,  22,   0,  23,  23,  23,   0,  24,  24,  24,   0,  25,  25,  25,   0,  26,  26,  26,   0,  27,  27,  27,   0,  28,  28,  28,   0,  29,  29,  29,   0,  30,  30,  30,   0,  31,  31,  31,   0,  32,  32,  32,   0,  33,  33,  33,   0,  34,  34,
			  34,   0,  35,  35,  35,   0,  36,  36,  36,   0,  37,  37,  37,   0,  38,  38,  38,   0,  39,  39,  39,   0,  40,  40,  40,   0,  41,  41,  41,   0,  42,  42,  42,   0,  43,  43,  43,   0,  44,  44,  44,   0,  45,  45,  45,   0,  46,  46,  46,   0,  47,  47,  47,   0,  48,  48,  48,   0,  49,  49,  49,   0,  50,  50,
			  50,   0,  51,  51,  51,   0,  52,  52,  52,   0,  53,  53,  53,   0,  54,  54,  54,   0,  55,  55,  55,   0,  56,  56,  56,   0,  57,  57,  57,   0,  58,  58,  58,   0,  59,  59,  59,   0,  60,  60,  60,   0,  61,  61,  61,   0,  62,  62,  62,   0,  63,  63,  63,   0,  64,  64,  64,   0,  65,  65,  65,   0,  66,  66,
			  66,   0,  67,  67,  67,   0,  68,  68,  68,   0,  69,  69,  69,   0,  70,  70,  70,   0,  71,  71,  71,   0,  72,  72,  72,   0,  73,  73,  73,   0,  74,  74,  74,   0,  75,  75,  75,   0,  76,  76,  76,   0,  77,  77,  77,   0,  78,  78,  78,   0,  79,  79,  79,   0,  80,  80,  80,   0,  81,  81,  81,   0,  82,  82,
			  82,   0,  83,  83,  83,   0,  84,  84,  84,   0,  85,  85,  85,   0,  86,  86,  86,   0,  87,  87,  87,   0,  88,  88,  88,   0,  89,  89,  89,   0,  90,  90,  90,   0,  91,  91,  91,   0,  92,  92,  92,   0,  93,  93,  93,   0,  94,  94,  94,   0,  95,  95,  95,   0,  96,  96,  96,   0,  97,  97,  97,   0,  98,  98,
			  98,   0,  99,  99,  99,   0, 100, 100, 100,   0, 101, 101, 101,   0, 102, 102, 102,   0, 103, 103, 103,   0, 104, 104, 104,   0, 105, 105, 105,   0, 106, 106, 106,   0, 107, 107, 107,   0, 108, 108, 108,   0, 109, 109, 109,   0, 110, 110, 110,   0, 111, 111, 111,   0, 112, 112, 112,   0, 113, 113, 113,   0, 114, 114,
			 114,   0, 115, 115, 115,   0, 116, 116, 116,   0, 117, 117, 117,   0, 118, 118, 118,   0, 119, 119, 119,   0, 120, 120, 120,   0, 121, 121, 121,   0, 122, 122, 122,   0, 123, 123, 123,   0, 124, 124, 124,   0, 125, 125, 125,   0, 126, 126, 126,   0, 127, 127, 127,   0, 128, 128, 128,   0, 129, 129, 129,   0, 130, 130,
			 130,   0, 131, 131, 131,   0, 132, 132, 132,   0, 133, 133, 133,   0, 134, 134, 134,   0, 135, 135, 135,   0, 136, 136, 136,   0, 137, 137, 137,   0, 138, 138, 138,   0, 139, 139, 139,   0, 140, 140, 140,   0, 141, 141, 141,   0, 142, 142, 142,   0, 143, 143, 143,   0, 144, 144, 144,   0, 145, 145, 145,   0, 146, 146,
			 146,   0, 147, 147, 147,   0, 148, 148, 148,   0, 149, 149, 149,   0, 150, 150, 150,   0, 151, 151, 151,   0, 152, 152, 152,   0, 153, 153, 153,   0, 154, 154, 154,   0, 155, 155, 155,   0, 156, 156, 156,   0, 157, 157, 157,   0, 158, 158, 158,   0, 159, 159, 159,   0, 160, 160, 160,   0, 161, 161, 161,   0, 162, 162,
			 162,   0, 163, 163, 163,   0, 164, 164, 164,   0, 165, 165, 165,   0, 166, 166, 166,   0, 167, 167, 167,   0, 168, 168, 168,   0, 169, 169, 169,   0, 170, 170, 170,   0, 171, 171, 171,   0, 172, 172, 172,   0, 173, 173, 173,   0, 174, 174, 174,   0, 175, 175, 175,   0, 176, 176, 176,   0, 177, 177, 177,   0, 178, 178,
			 178,   0, 179, 179, 179,   0, 180, 180, 180,   0, 181, 181, 181,   0, 182, 182, 182,   0, 183, 183, 183,   0, 184, 184, 184,   0, 185, 185, 185,   0, 186, 186, 186,   0, 187, 187, 187,   0, 188, 188, 188,   0, 189, 189, 189,   0, 190, 190, 190,   0, 191, 191, 191,   0, 192, 192, 192,   0, 193, 193, 193,   0, 194, 194,
			 194,   0, 195, 195, 195,   0, 196, 196, 196,   0, 197, 197, 197,   0, 198, 198, 198,   0, 199, 199, 199,   0, 200, 200, 200,   0, 201, 201, 201,   0, 202, 202, 202,   0, 203, 203, 203,   0, 204, 204, 204,   0, 205, 205, 205,   0, 206, 206, 206,   0, 207, 207, 207,   0, 208, 208, 208,   0, 209, 209, 209,   0, 210, 210,
			 210,   0, 211, 211, 211,   0, 212, 212, 212,   0, 213, 213, 213,   0, 214, 214, 214,   0, 215, 215, 215,   0, 216, 216, 216,   0, 217, 217, 217,   0, 218, 218, 218,   0, 219, 219, 219,   0, 220, 220, 220,   0, 221, 221, 221,   0, 222, 222, 222,   0, 223, 223, 223,   0, 224, 224, 224,   0, 225, 225, 225,   0, 226, 226,
			 226,   0, 227, 227, 227,   0, 228, 228, 228,   0, 229, 229, 229,   0, 230, 230, 230,   0, 231, 231, 231,   0, 232, 232, 232,   0, 233, 233, 233,   0, 234, 234, 234,   0, 235, 235, 235,   0, 236, 236, 236,   0, 237, 237, 237,   0, 238, 238, 238,   0, 239, 239, 239,   0, 240, 240, 240,   0, 241, 241, 241,   0, 242, 242,
			 242,   0, 243, 243, 243,   0, 244, 244, 244,   0, 245, 245, 245,   0, 246, 246, 246,   0, 247, 247, 247,   0, 248, 248, 248,   0, 249, 249, 249,   0, 250, 250, 250,   0, 251, 251, 251,   0, 252, 252, 252,   0, 253, 253, 253,   0, 254, 254, 254,   0, 255, 255, 255,   0	
		};
		if (palettes) {
			const const_data_view& pdv = palettes->at(0);
			for (unsigned int i=0; i<256; ++i) {
				bmp_header[4*i+54] = pdv(i).get<unsigned char>(2);
				bmp_header[4*i+55] = pdv(i).get<unsigned char>(1);
				bmp_header[4*i+56] = pdv(i).get<unsigned char>(0);
			}
		}
		((unsigned short*)bmp_header)[9]  = width;
		((unsigned short*)bmp_header)[11] = height;
		bool success = fwrite(bmp_header, 1, 1078, fp) == 1078;
		if (success) {
			unsigned int line_padding =  unsigned(packing_info::align(width,4)) - width;
			data += (height-1)*dv.get_format()->get_width();
			for (unsigned short y = 0; success && y < height; ++y) {
				if (!(success = fwrite(data, 1, width, fp) == width) ) {
					last_error = "bmp write error";
					break;
				}
				data -= dv.get_format()->get_width();
				success = !(line_padding && fwrite(bmp_header+50, 1, line_padding, fp) != line_padding);
			}
		}
		else
			last_error = "could not write bmp header";
		return success;
	}
	return false;
}

/// close image [stream]
bool bmp_writer::close()
{
	return fclose(fp) == 0;
}

cgv::base::object_registration<bmp_writer> bwr("register bmp writer");

		}
	}
}
