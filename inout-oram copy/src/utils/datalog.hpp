#pragma once

#include "ints.hpp"
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
#include <type_traits>
#include <vector>

struct DataRow {
  std::vector<std::string> column_names;
  std::vector<std::string> values;

  // default constructor
  DataRow() {}

  // copy constructor
  DataRow(const DataRow& other) : column_names(other.column_names), values(other.values) {}

  // std::string
  void add_column(const std::string& name, const std::string& value) {
    column_names.push_back(name);
    values.push_back(value);
  }

  // const char*
  void add_column(const std::string& name, const char* value) {
    column_names.push_back(std::string(name));
    values.push_back(value);
  }

  // integer types
  template <typename T> void add_column(const std::string& name, T value) {
    static_assert(std::is_integral<T>::value, "T must be an integral type");
    column_names.push_back(name);
    values.push_back(std::to_string(value));
  }

  // double
  void add_column(const std::string& name, double value) {
    column_names.push_back(name);
    values.push_back(tfm::format("%.12f", value));
  }
};

class DataLog {
public:
  std::string name;
  std::vector<DataRow> rows;
  size_t n_columns = 0;

  DataLog(std::string name, std::vector<DataRow> rows) : name(name), rows(rows) {}
  DataLog(std::string name) : DataLog(name, std::vector<DataRow>()) {}

  bool empty() const {
    return rows.empty();
  }

  void add_row(DataRow& row) {
    if (n_columns == 0) {
      n_columns = row.column_names.size();
    } else {
      if (n_columns != row.column_names.size()) {
        throw std::runtime_error("column count mismatch");
      }
    }
    rows.push_back(row);
  }

  std::string to_string() {
    std::stringstream ss;
    // write column names
    for (size_t i = 0; i < rows[0].column_names.size(); i++) {
      ss << rows[0].column_names[i];
      if (i < rows[0].column_names.size() - 1) {
        ss << ",";
      }
    }
    ss << std::endl;

    // write rows
    for (size_t i = 0; i < rows.size(); i++) {
      for (size_t j = 0; j < rows[i].values.size(); j++) {
        ss << rows[i].values[j];
        if (j < rows[i].values.size() - 1) {
          ss << ",";
        }
      }
      ss << std::endl;
    }
    return ss.str();
  }

  void save_to(const std::string& filename) {
    std::ofstream ouf(filename);
    if (!ouf.is_open()) {
      throw std::runtime_error("failed to open file for writing");
    }
    ouf << to_string();
  }
};
