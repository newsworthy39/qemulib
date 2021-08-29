#include <qemu-images.hpp>

std::map<std::string, std::string> machineimages = {};

/**
 * Helper functions 
 */
// https://stackoverflow.com/questions/2342162/stdstring-formatting-like-sprintf
template <typename... Args>
std::string m1_string_format(const std::string &format, Args... args)
{
  int size_s = std::snprintf(nullptr, 0, format.c_str(), args...) + 1; // Extra space for '\0'
  if (size_s <= 0)
  {
    throw std::runtime_error("Error during formatting.");
  }
  auto size = static_cast<size_t>(size_s);
  auto buf = std::make_unique<char[]>(size);
  std::snprintf(buf.get(), size, format.c_str(), args...);
  return std::string(buf.get(), buf.get() + size - 1); // We don't want the '\0' inside
}

bool m1_fileExists(const std::string &filename)
{
  struct stat buffer;
  return (stat(filename.c_str(), &buffer) == 0);
}

/**
 * qemuListImages(const std::string& databasepath.)
 * returns a map of strings, strings with images, registered in the databasepath.
 */
std::map<std::string, std::string> qemuListImages(const std::string& databasepath)
{
  std::string error;
  json11::Json obj = qemuGetFileContents(databasepath.c_str(), error);
  auto t1 = obj.object_items();
  std::vector<json11::Json> t2 = t1["images"].array_items();
  for (std::vector<json11::Json>::iterator it = t2.begin(); it != t2.end(); it++)
  {
    std::map<std::string, json11::Json> objitems = it->object_items();
    for (const auto &[key, value] : objitems) {
      auto t3 = value.object_items();
      
      // std::cout << t3["default"].string_value() << std::endl;
      
      machineimages.insert(std::pair<std::string, std::string>(key, t3["path"].string_value()));
    }
  }

  return machineimages;
}

void qemuRegisterImage(std::string model, std::string type, std::string path)
{
  std::string machine = m1_string_format("%s%s", model, type);
  machineimages.insert(std::pair<std::string, std::string>(machine, path));
}

/**
 * std::string get_file_contents(const char *filename)
   returns std::string
*/
json11::Json qemuGetFileContents(const std::string &filename, std::string &err)
{
  std::ifstream in(filename.c_str(), std::ios::in | std::ios::binary);
  if (in)
  {
    std::ostringstream contents;
    contents << in.rdbuf();
    in.close();
    json11::Json obj = json11::Json::parse(contents.str(), err);
    return obj;
  }
  throw(errno);
}

void qemuWriteFileContents(const std::string &filename, const json11::Json& jsonobj)
{
  std::filebuf fb;
  fb.open(filename.c_str(), std::ios::out | std::ios::binary);
  std::ostream out(&fb);
  out << jsonobj.dump();
  fb.close();
}
