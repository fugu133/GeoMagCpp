/**
 * @file Model.hpp
 * @author fugu133
 * @brief IGRF-13 Model
 * @ref https://www.ngdc.noaa.gov/IAGA/vmod/igrf.html
 * @version 0.1
 * @date 2023-12-22
 *
 * @copyright Copyright (c) 2023
 *
 */

#pragma once
#include <algorithm>
#include <array>
#include <fstream>
#include <iterator>
#include <sstream>
#include <vector>

#include "DateTime.hpp"
#include "Macro.hpp"

GEOMAG_NAMESPACE_BEGIN
/**
 * @brief モデルの種類
 *
 */
enum class ModelType {
	Unknown = -1, // Unknown Model
	Dgrf = 0,	  // Definitive Geomagnetic Reference Field
	Igrf,		  // International Geomagnetic Reference Field
	Sv,			  // Secular Variation
	Interpolated, // Interpolated Model
	Extrapolated, // Extrapolated Model
};

/**
 * @brief モデルの構造体
 * @ref https://www.ngdc.noaa.gov/IAGA/vmod/igrf.html
 *
 */
struct Model {
	static constexpr std::size_t max_degree = 13;
	static constexpr std::size_t max_order = 13;
	static constexpr std::size_t max_coefficient_size = (max_degree * (max_degree + 2) + 1);

	DateTime epoch;										   // epoch of the model
	ModelType type;										   // type of the model
	std::array<double, max_coefficient_size> coefficients; // g/h coefficients of the model

	Model() : epoch(), type(ModelType::Unknown), coefficients{0} {}
	Model(const DateTime& dt, ModelType t, const std::array<double, max_coefficient_size>& coeff)
	  : epoch(dt), type(t), coefficients(coeff) {}
};

/**
 * @brief モデルセット
 *
 */
class ModelSet {
  public:
	/**
	 * @brief Construct a new Model Set object
	 * @remark デフォルトコンストラクタは最新のモデルを読み込む
	 *
	 */
	ModelSet();

	/**
	 * @brief Construct a new Model Set object
	 *
	 * @param is モデルファイルのストリーム
	 */
	ModelSet(std::istream& is) { read(is); }

	/**
	 * @brief Construct a new Model Set object
	 *
	 * @param models モデルの配列
	 */
	ModelSet(const std::vector<Model>& models) : m_models(models) {}

	/**
	 * @brief Construct a new Model Set object
	 *
	 * @param ms モデルセット
	 */
	ModelSet(const ModelSet& ms) : m_models(ms.m_models) {}

	/**
	 * @brief 必要なモデルを選択する
	 *
	 * @param dt 欲しいモデルのエポック
	 * @param last 欲しいモデルのエポックよりも前のモデル
	 * @param next 欲しいモデルのエポックよりも先のモデル
	 */
	void select(const DateTime& dt, Model& last, Model& next) const {
		if (m_models.empty()) {
			throw std::runtime_error("ModelSet is empty.");
		} else {
			// dt < models[i].epoch < models[i+1].epochとなる最大のiを探す
			auto it =
			  std::lower_bound(m_models.begin(), m_models.end(), dt, [](const Model& m, const DateTime& dt) { return m.epoch < dt; });

			if (it == m_models.end()) {
				throw std::runtime_error("ModelSet: no model is found.");
			} else {
				last = *(it - 1);
				next = *it;
			}
		}
	}

	const Model& operator[](std::size_t i) const { return m_models[i]; }
	const Model& at(std::size_t i) const { return m_models.at(i); }
	std::size_t size() const { return m_models.size(); }

	auto begin() const { return m_models.begin(); }
	auto end() const { return m_models.end(); }

  private:
	static constexpr char* model_file_comment_header = (char*)"#";
	static constexpr char* model_file_model_header = (char*)"c/s";
	static constexpr char* model_file_epoch_header = (char*)"g/h";
	static constexpr char* model_file_g_coeff_header = (char*)"g";
	static constexpr char* model_file_h_coeff_header = (char*)"h";

	std::vector<Model> m_models;

	enum class ModelFileRowType {
		Unknown = -1,
		Comment = 0,
		ModelType,
		Epoch,
		GCoeffecient,
		HCoeffecient,
	};

	ModelFileRowType getModelFileRowType(std::string& line) const {
		if (line.empty()) { // 空行
			return ModelFileRowType::Unknown;
		} else if (line.find(model_file_comment_header) != std::string::npos) { // コメント行
			return ModelFileRowType::Comment;
		} else if (line.find(model_file_model_header) != std::string::npos) { // モデル行 c/s で始まる
			return ModelFileRowType::ModelType;
		} else if (line.find(model_file_epoch_header) != std::string::npos) { // エポック行 g/h で始まる
			return ModelFileRowType::Epoch;
		} else if (line.find(model_file_g_coeff_header) != std::string::npos) { // エポック行 g/h で始まる
			return ModelFileRowType::GCoeffecient;
		} else if (line.find(model_file_h_coeff_header) != std::string::npos) { // エポック行 g/h で始まる
			return ModelFileRowType::HCoeffecient;
		} else {
			return ModelFileRowType::Unknown;
		}
	}

	ModelType getModelType(const std::string& elem) const {
		if (elem == "DGRF") {
			return ModelType::Dgrf;
		} else if (elem == "IGRF") {
			return ModelType::Igrf;
		} else if (elem == "SV") {
			return ModelType::Sv;
		} else {
			return ModelType::Unknown;
		}
	}

	void read(std::istream& is) {
		std::string line;
		std::size_t c_i = 0; // coefficient index

		while (std::getline(is, line)) {
			switch (getModelFileRowType(line)) {
				case ModelFileRowType::Comment: break;
				case ModelFileRowType::ModelType: { // DRGF, IGRF, SV
					std::istringstream iss(line);
					std::istream_iterator<std::string> begin(iss), end;
					while (begin != end) {
						auto& elm = *begin;
						auto type = getModelType(elm);
						if (type != ModelType::Unknown) {
							m_models.emplace_back();
							m_models.back().type = type;
						}
						begin++;
					}
					break;
				}

				case ModelFileRowType::Epoch: {
					std::istringstream iss(line);
					std::istream_iterator<std::string> begin(iss), end;
					std::size_t i = 0;

					while (begin != end) {
						auto& elm = *begin;
						if (elm.length() >= sizeof("yyyy.y") - 1) { // yyyy.y もしくは yyyy-yy の形式
							auto range_bound = elm.find("-");
							try {
								if (range_bound != std::string::npos) {
									auto lower_range = elm.substr(0, range_bound);
									auto upper_range = elm.substr(range_bound + 1, elm.length() - range_bound);
									m_models[i].epoch = DateTime(
									  std::stoi(lower_range.substr(0, lower_range.length() - upper_range.length()) + upper_range), 1);
								} else {
									m_models[i].epoch = DateTime(std::stoi(elm), 1);
								}
								i++;
							} catch (...) {
							}
						}
						begin++;
					}
				} break;

				case ModelFileRowType::GCoeffecient:
				case ModelFileRowType::HCoeffecient: {
					std::istringstream iss(line);
					std::istream_iterator<std::string> begin(iss), end;
					std::size_t m_i = 0;
					std::size_t colum = 0;

					while (begin != end) {
						auto& elm = *begin;
						if (colum >= 3) { // 3列目以降は係数
							try {
								m_models[m_i].coefficients[c_i] = std::stod(elm);
								m_i++;
							} catch (...) {
							}
						}
						begin++;
						colum++;
					}

					if (m_i == m_models.size()) {
						c_i++;
					}
				} break;
				default: break;
			}
		}
	}
};

/**
 * @brief IGRF-13 Model
 *
 */
inline ModelSet::ModelSet()
  : m_models{std::vector<Model>{
	  {
		{"1900-01-01T00:00:00.000000Z"},
		ModelType::Igrf,
		{-31543, -2298, 5922, -677, 2905, -1061, 924, 1121, 1022, -1469, -330, 1256, 3,	 572, 523, 876, 628, 195, 660,	-69, -361, -210,
		 134,	 -75,	-184, 328,	-210, 264,	 53,  5,	-33,  -86,	 -124, -16,	 3,	 63,  61,  -9,	-11, 83,  -217, 2,	 -58,  -35,
		 59,	 36,	-90,  -69,	70,	  -55,	 -45, 0,	-13,  34,	 -10,  -41,	 -1, -21, 28,  18,	-12, 6,	  -22,	11,	 8,	   8,
		 -4,	 -14,	-9,	  7,	1,	  -13,	 2,	  5,	-9,	  16,	 5,	   -5,	 8,	 -18, 8,   10,	-20, 1,	  14,	-11, 5,	   12,
		 -3,	 1,		-2,	  -2,	8,	  2,	 10,  -1,	-2,	  -1,	 2,	   -3,	 -4, 2,	  2,   1,	-5,	 2,	  -2,	6,	 6,	   -4,
		 4,		 0,		0,	  -2,	2,	  4,	 2,	  0,	0,	  -6,	 0,	   0,	 0,	 0,	  0,   0,	0,	 0,	  0,	0,	 0,	   0,
		 0,		 0,		0,	  0,	0,	  0,	 0,	  0,	0,	  0,	 0,	   0,	 0,	 0,	  0,   0,	0,	 0,	  0,	0,	 0,	   0,
		 0,		 0,		0,	  0,	0,	  0,	 0,	  0,	0,	  0,	 0,	   0,	 0,	 0,	  0,   0,	0,	 0,	  0,	0,	 0,	   0,
		 0,		 0,		0,	  0,	0,	  0,	 0,	  0,	0,	  0,	 0,	   0,	 0,	 0,	  0,   0,	0,	 0,	  0,	0},
	  },
	  {
		{"1905-01-01T00:00:00.000000Z"},
		ModelType::Igrf,
		{-31464, -2298, 5909, -728, 2928, -1086, 1041, 1065, 1037, -1494, -357, 1239, 34, 635, 480, 880, 643, 203, 653,	 -77, -380, -201,
		 146,	 -65,	-192, 328,	-193, 259,	 56,   -1,	 -32,  -93,	  -125, -26,  11, 62,  60,	-7,	 -11, 86,  -221, 4,	  -57,	-32,
		 57,	 32,	-92,  -67,	70,	  -54,	 -46,  0,	 -14,  33,	  -11,	-41,  0,  -20, 28,	18,	 -12, 6,   -22,	 11,  8,	8,
		 -4,	 -15,	-9,	  7,	1,	  -13,	 2,	   5,	 -8,   16,	  5,	-5,	  8,  -18, 8,	10,	 -20, 1,   14,	 -11, 5,	12,
		 -3,	 1,		-2,	  -2,	8,	  2,	 10,   0,	 -2,   -1,	  2,	-3,	  -4, 2,   2,	1,	 -5,  2,   -2,	 6,	  6,	-4,
		 4,		 0,		0,	  -2,	2,	  4,	 2,	   0,	 0,	   -6,	  0,	0,	  0,  0,   0,	0,	 0,	  0,   0,	 0,	  0,	0,
		 0,		 0,		0,	  0,	0,	  0,	 0,	   0,	 0,	   0,	  0,	0,	  0,  0,   0,	0,	 0,	  0,   0,	 0,	  0,	0,
		 0,		 0,		0,	  0,	0,	  0,	 0,	   0,	 0,	   0,	  0,	0,	  0,  0,   0,	0,	 0,	  0,   0,	 0,	  0,	0,
		 0,		 0,		0,	  0,	0,	  0,	 0,	   0,	 0,	   0,	  0,	0,	  0,  0,   0,	0,	 0,	  0,   0,	 0},
	  },
	  {
		{"1910-01-01T00:00:00.000000Z"},
		ModelType::Igrf,
		{-31354, -2297, 5898, -769, 2948, -1128, 1176, 1000, 1058, -1524, -389, 1223, 62, 705, 425, 884, 660, 211, 644,	 -90, -400, -189,
		 160,	 -55,	-201, 327,	-172, 253,	 57,   -9,	 -33,  -102,  -126, -38,  21, 62,  58,	-5,	 -11, 89,  -224, 5,	  -54,	-29,
		 54,	 28,	-95,  -65,	71,	  -54,	 -47,  1,	 -14,  32,	  -12,	-40,  1,  -19, 28,	18,	 -13, 6,   -22,	 11,  8,	8,
		 -4,	 -15,	-9,	  6,	1,	  -13,	 2,	   5,	 -8,   16,	  5,	-5,	  8,  -18, 8,	10,	 -20, 1,   14,	 -11, 5,	12,
		 -3,	 1,		-2,	  -2,	8,	  2,	 10,   0,	 -2,   -1,	  2,	-3,	  -4, 2,   2,	1,	 -5,  2,   -2,	 6,	  6,	-4,
		 4,		 0,		0,	  -2,	2,	  4,	 2,	   0,	 0,	   -6,	  0,	0,	  0,  0,   0,	0,	 0,	  0,   0,	 0,	  0,	0,
		 0,		 0,		0,	  0,	0,	  0,	 0,	   0,	 0,	   0,	  0,	0,	  0,  0,   0,	0,	 0,	  0,   0,	 0,	  0,	0,
		 0,		 0,		0,	  0,	0,	  0,	 0,	   0,	 0,	   0,	  0,	0,	  0,  0,   0,	0,	 0,	  0,   0,	 0,	  0,	0,
		 0,		 0,		0,	  0,	0,	  0,	 0,	   0,	 0,	   0,	  0,	0,	  0,  0,   0,	0,	 0,	  0,   0,	 0},
	  },
	  {
		{"1915-01-01T00:00:00.000000Z"},
		ModelType::Igrf,
		{-31212, -2306, 5875, -802, 2956, -1191, 1309, 917, 1084, -1559, -421, 1212, 84, 778, 360, 887, 678, 218, 631,	-109, -416, -173,
		 178,	 -51,	-211, 327,	-148, 245,	 58,   -16, -34,  -111,	 -126, -51,	 32, 61,  57,  -2,	-10, 93,  -228, 8,	  -51,	-26,
		 49,	 23,	-98,  -62,	72,	  -54,	 -48,  2,	-14,  31,	 -12,  -38,	 2,	 -18, 28,  19,	-15, 6,	  -22,	11,	  8,	8,
		 -4,	 -15,	-9,	  6,	2,	  -13,	 3,	   5,	-8,	  16,	 6,	   -5,	 8,	 -18, 8,   10,	-20, 1,	  14,	-11,  5,	12,
		 -3,	 1,		-2,	  -2,	8,	  2,	 10,   0,	-2,	  -1,	 2,	   -3,	 -4, 2,	  2,   1,	-5,	 2,	  -2,	6,	  6,	-4,
		 4,		 0,		0,	  -2,	1,	  4,	 2,	   0,	0,	  -6,	 0,	   0,	 0,	 0,	  0,   0,	0,	 0,	  0,	0,	  0,	0,
		 0,		 0,		0,	  0,	0,	  0,	 0,	   0,	0,	  0,	 0,	   0,	 0,	 0,	  0,   0,	0,	 0,	  0,	0,	  0,	0,
		 0,		 0,		0,	  0,	0,	  0,	 0,	   0,	0,	  0,	 0,	   0,	 0,	 0,	  0,   0,	0,	 0,	  0,	0,	  0,	0,
		 0,		 0,		0,	  0,	0,	  0,	 0,	   0,	0,	  0,	 0,	   0,	 0,	 0,	  0,   0,	0,	 0,	  0,	0},
	  },
	  {
		{"1920-01-01T00:00:00.000000Z"},
		ModelType::Igrf,
		{-31060, -2317, 5845, -839, 2959, -1259, 1407, 823, 1111, -1600, -445, 1205, 103, 839, 293, 889, 695, 220, 616,	 -134, -424, -153,
		 199,	 -57,	-221, 326,	-122, 236,	 58,   -23, -38,  -119,	 -125, -62,	 43,  61,  55,	0,	 -10, 96,  -233, 11,   -46,	 -22,
		 44,	 18,	-101, -57,	73,	  -54,	 -49,  2,	-14,  29,	 -13,  -37,	 4,	  -16, 28,	19,	 -16, 6,   -22,	 11,   7,	 8,
		 -3,	 -15,	-9,	  6,	2,	  -14,	 4,	   5,	-7,	  17,	 6,	   -5,	 8,	  -19, 8,	10,	 -20, 1,   14,	 -11,  5,	 12,
		 -3,	 1,		-2,	  -2,	9,	  2,	 10,   0,	-2,	  -1,	 2,	   -3,	 -4,  2,   2,	1,	 -5,  2,   -2,	 6,	   6,	 -4,
		 4,		 0,		0,	  -2,	1,	  4,	 3,	   0,	0,	  -6,	 0,	   0,	 0,	  0,   0,	0,	 0,	  0,   0,	 0,	   0,	 0,
		 0,		 0,		0,	  0,	0,	  0,	 0,	   0,	0,	  0,	 0,	   0,	 0,	  0,   0,	0,	 0,	  0,   0,	 0,	   0,	 0,
		 0,		 0,		0,	  0,	0,	  0,	 0,	   0,	0,	  0,	 0,	   0,	 0,	  0,   0,	0,	 0,	  0,   0,	 0,	   0,	 0,
		 0,		 0,		0,	  0,	0,	  0,	 0,	   0,	0,	  0,	 0,	   0,	 0,	  0,   0,	0,	 0,	  0,   0,	 0},
	  },
	  {
		{"1925-01-01T00:00:00.000000Z"},
		ModelType::Igrf,
		{-30926, -2318, 5817, -893, 2969, -1334, 1471, 728, 1140, -1645, -462, 1202, 119, 881, 229, 891, 711, 216, 601,	 -163, -426, -130,
		 217,	 -70,	-230, 326,	-96,  226,	 58,   -28, -44,  -125,	 -122, -69,	 51,  61,  54,	3,	 -9,  99,  -238, 14,   -40,	 -18,
		 39,	 13,	-103, -52,	73,	  -54,	 -50,  3,	-14,  27,	 -14,  -35,	 5,	  -14, 29,	19,	 -17, 6,   -21,	 11,   7,	 8,
		 -3,	 -15,	-9,	  6,	2,	  -14,	 4,	   5,	-7,	  17,	 7,	   -5,	 8,	  -19, 8,	10,	 -20, 1,   14,	 -11,  5,	 12,
		 -3,	 1,		-2,	  -2,	9,	  2,	 10,   0,	-2,	  -1,	 2,	   -3,	 -4,  2,   2,	1,	 -5,  2,   -2,	 6,	   6,	 -4,
		 4,		 0,		0,	  -2,	1,	  4,	 3,	   0,	0,	  -6,	 0,	   0,	 0,	  0,   0,	0,	 0,	  0,   0,	 0,	   0,	 0,
		 0,		 0,		0,	  0,	0,	  0,	 0,	   0,	0,	  0,	 0,	   0,	 0,	  0,   0,	0,	 0,	  0,   0,	 0,	   0,	 0,
		 0,		 0,		0,	  0,	0,	  0,	 0,	   0,	0,	  0,	 0,	   0,	 0,	  0,   0,	0,	 0,	  0,   0,	 0,	   0,	 0,
		 0,		 0,		0,	  0,	0,	  0,	 0,	   0,	0,	  0,	 0,	   0,	 0,	  0,   0,	0,	 0,	  0,   0,	 0},
	  },
	  {
		{"1930-01-01T00:00:00.000000Z"},
		ModelType::Igrf,
		{-30805, -2316, 5808, -951, 2980, -1424, 1517, 644, 1172, -1692, -480, 1205, 133, 907, 166, 896, 727, 205, 584,	 -195, -422, -109,
		 234,	 -90,	-237, 327,	-72,  218,	 60,   -32, -53,  -131,	 -118, -74,	 58,  60,  53,	4,	 -9,  102, -242, 19,   -32,	 -16,
		 32,	 8,		-104, -46,	74,	  -54,	 -51,  4,	-15,  25,	 -14,  -34,	 6,	  -12, 29,	18,	 -18, 6,   -20,	 11,   7,	 8,
		 -3,	 -15,	-9,	  5,	2,	  -14,	 5,	   5,	-6,	  18,	 8,	   -5,	 8,	  -19, 8,	10,	 -20, 1,   14,	 -12,  5,	 12,
		 -3,	 1,		-2,	  -2,	9,	  3,	 10,   0,	-2,	  -2,	 2,	   -3,	 -4,  2,   2,	1,	 -5,  2,   -2,	 6,	   6,	 -4,
		 4,		 0,		0,	  -2,	1,	  4,	 3,	   0,	0,	  -6,	 0,	   0,	 0,	  0,   0,	0,	 0,	  0,   0,	 0,	   0,	 0,
		 0,		 0,		0,	  0,	0,	  0,	 0,	   0,	0,	  0,	 0,	   0,	 0,	  0,   0,	0,	 0,	  0,   0,	 0,	   0,	 0,
		 0,		 0,		0,	  0,	0,	  0,	 0,	   0,	0,	  0,	 0,	   0,	 0,	  0,   0,	0,	 0,	  0,   0,	 0,	   0,	 0,
		 0,		 0,		0,	  0,	0,	  0,	 0,	   0,	0,	  0,	 0,	   0,	 0,	  0,   0,	0,	 0,	  0,   0,	 0},
	  },
	  {
		{"1935-01-01T00:00:00.000000Z"},
		ModelType::Igrf,
		{-30715, -2306, 5812, -1018, 2984, -1520, 1550, 586, 1206, -1740, -494, 1215, 146, 918, 101, 903, 744, 188, 565,  -226, -415, -90,
		 249,	 -114,	-241, 329,	 -51,  211,	  64,	-33, -64,  -136,  -115, -76,  64,  59,	53,	 4,	  -8,  104, -246, 25,	-25,  -15,
		 25,	 4,		-106, -40,	 74,   -53,	  -52,	4,	 -17,  23,	  -14,	-33,  7,   -11, 29,	 18,  -19, 6,	-19,  11,	7,	  8,
		 -3,	 -15,	-9,	  5,	 1,	   -15,	  6,	5,	 -6,   18,	  8,	-5,	  7,   -19, 8,	 10,  -20, 1,	15,	  -12,	5,	  11,
		 -3,	 1,		-3,	  -2,	 9,	   3,	  11,	0,	 -2,   -2,	  2,	-3,	  -4,  2,	2,	 1,	  -5,  2,	-2,	  6,	6,	  -4,
		 4,		 0,		0,	  -1,	 2,	   4,	  3,	0,	 0,	   -6,	  0,	0,	  0,   0,	0,	 0,	  0,   0,	0,	  0,	0,	  0,
		 0,		 0,		0,	  0,	 0,	   0,	  0,	0,	 0,	   0,	  0,	0,	  0,   0,	0,	 0,	  0,   0,	0,	  0,	0,	  0,
		 0,		 0,		0,	  0,	 0,	   0,	  0,	0,	 0,	   0,	  0,	0,	  0,   0,	0,	 0,	  0,   0,	0,	  0,	0,	  0,
		 0,		 0,		0,	  0,	 0,	   0,	  0,	0,	 0,	   0,	  0,	0,	  0,   0,	0,	 0,	  0,   0,	0,	  0},
	  },
	  {
		{"1940-01-01T00:00:00.000000Z"},
		ModelType::Igrf,
		{-30654, -2292, 5821, -1106, 2981, -1614, 1566, 528, 1240, -1790, -499, 1232, 163, 916, 43, 914, 762, 169, 550,	 -252, -405, -72,
		 265,	 -141,	-241, 334,	 -33,  208,	  71,	-33, -75,  -141,  -113, -76,  69,  57,	54, 4,	 -7,  105, -249, 33,   -18,	 -15,
		 18,	 0,		-107, -33,	 74,   -53,	  -52,	4,	 -18,  20,	  -14,	-31,  7,   -9,	29, 17,	 -20, 5,   -19,	 11,   7,	 8,
		 -3,	 -14,	-10,  5,	 1,	   -15,	  6,	5,	 -5,   19,	  9,	-5,	  7,   -19, 8,	10,	 -21, 1,   15,	 -12,  5,	 11,
		 -3,	 1,		-3,	  -2,	 9,	   3,	  11,	1,	 -2,   -2,	  2,	-3,	  -4,  2,	2,	1,	 -5,  2,   -2,	 6,	   6,	 -4,
		 4,		 0,		0,	  -1,	 2,	   4,	  3,	0,	 0,	   -6,	  0,	0,	  0,   0,	0,	0,	 0,	  0,   0,	 0,	   0,	 0,
		 0,		 0,		0,	  0,	 0,	   0,	  0,	0,	 0,	   0,	  0,	0,	  0,   0,	0,	0,	 0,	  0,   0,	 0,	   0,	 0,
		 0,		 0,		0,	  0,	 0,	   0,	  0,	0,	 0,	   0,	  0,	0,	  0,   0,	0,	0,	 0,	  0,   0,	 0,	   0,	 0,
		 0,		 0,		0,	  0,	 0,	   0,	  0,	0,	 0,	   0,	  0,	0,	  0,   0,	0,	0,	 0,	  0,   0,	 0},
	  },
	  {
		{"1945-01-01T00:00:00.000000Z"},
		ModelType::Dgrf,
		{-30594, -2285, 5810, -1244, 2990, -1702, 1578, 477, 1282, -1834, -499, 1255, 186, 913, -11, 944, 776, 144, 544,  -276, -421, -55,
		 304,	 -178,	-253, 346,	 -12,  194,	  95,	-20, -67,  -142,  -119, -82,  82,  59,	57,	 6,	  6,   100, -246, 16,	-25,  -9,
		 21,	 -16,	-104, -39,	 70,   -40,	  -45,	0,	 -18,  0,	  2,	-29,  6,   -10, 28,	 15,  -17, 29,	-22,  13,	7,	  12,
		 -8,	 -21,	-5,	  -12,	 9,	   -7,	  7,	2,	 -10,  18,	  7,	3,	  2,   -11, 5,	 -21, -27, 1,	17,	  -11,	29,	  3,
		 -9,	 16,	4,	  -3,	 9,	   -4,	  6,	-3,	 1,	   -4,	  8,	-3,	  11,  5,	1,	 1,	  2,   -20, -5,	  -1,	-1,	  -6,
		 8,		 6,		-1,	  -4,	 -3,   -2,	  5,	0,	 -2,   -2,	  0,	0,	  0,   0,	0,	 0,	  0,   0,	0,	  0,	0,	  0,
		 0,		 0,		0,	  0,	 0,	   0,	  0,	0,	 0,	   0,	  0,	0,	  0,   0,	0,	 0,	  0,   0,	0,	  0,	0,	  0,
		 0,		 0,		0,	  0,	 0,	   0,	  0,	0,	 0,	   0,	  0,	0,	  0,   0,	0,	 0,	  0,   0,	0,	  0,	0,	  0,
		 0,		 0,		0,	  0,	 0,	   0,	  0,	0,	 0,	   0,	  0,	0,	  0,   0,	0,	 0,	  0,   0,	0,	  0},
	  },
	  {
		{"1950-01-01T00:00:00.000000Z"},
		ModelType::Dgrf,
		{-30554, -2250, 5815, -1341, 2998, -1810, 1576, 381, 1297, -1889, -476, 1274, 206, 896, -46, 954, 792, 136, 528,  -278, -408, -37,
		 303,	 -210,	-240, 349,	 3,	   211,	  103,	-20, -87,  -147,  -122, -76,  80,  54,	57,	 -1,  4,   99,	-247, 33,	-16,  -12,
		 12,	 -12,	-105, -30,	 65,   -55,	  -35,	2,	 -17,  1,	  0,	-40,  10,  -7,	36,	 5,	  -18, 19,	-16,  22,	15,	  5,
		 -4,	 -22,	-1,	  0,	 11,   -21,	  15,	-8,	 -13,  17,	  5,	-4,	  -1,  -17, 3,	 -7,  -24, -1,	19,	  -25,	12,	  10,
		 2,		 5,		2,	  -5,	 8,	   -2,	  8,	3,	 -11,  8,	  -7,	-8,	  4,   13,	-1,	 -2,  13,  -10, -4,	  2,	4,	  -3,
		 12,	 6,		3,	  -3,	 2,	   6,	  10,	11,	 3,	   8,	  0,	0,	  0,   0,	0,	 0,	  0,   0,	0,	  0,	0,	  0,
		 0,		 0,		0,	  0,	 0,	   0,	  0,	0,	 0,	   0,	  0,	0,	  0,   0,	0,	 0,	  0,   0,	0,	  0,	0,	  0,
		 0,		 0,		0,	  0,	 0,	   0,	  0,	0,	 0,	   0,	  0,	0,	  0,   0,	0,	 0,	  0,   0,	0,	  0,	0,	  0,
		 0,		 0,		0,	  0,	 0,	   0,	  0,	0,	 0,	   0,	  0,	0,	  0,   0,	0,	 0,	  0,   0,	0,	  0},
	  },
	  {
		{"1955-01-01T00:00:00.000000Z"},
		ModelType::Dgrf,
		{-30500, -2215, 5820, -1440, 3003, -1898, 1581, 291, 1302, -1944, -462, 1288, 216, 882, -83, 958, 796, 133, 510,  -274, -397, -23,
		 290,	 -230,	-229, 360,	 15,   230,	  110,	-23, -98,  -152,  -121, -69,  78,  47,	57,	 -9,  3,   96,	-247, 48,	-8,	  -16,
		 7,		 -12,	-107, -24,	 65,   -56,	  -50,	2,	 -24,  10,	  -4,	-32,  8,   -11, 28,	 9,	  -20, 18,	-18,  11,	9,	  10,
		 -6,	 -15,	-14,  5,	 6,	   -23,	  10,	3,	 -7,   23,	  6,	-4,	  9,   -13, 4,	 9,	  -11, -4,	12,	  -5,	7,	  2,
		 6,		 4,		-2,	  1,	 10,   2,	  7,	2,	 -6,   5,	  5,	-3,	  -5,  -4,	-1,	 0,	  2,   -8,	-3,	  -2,	7,	  -4,
		 4,		 1,		-2,	  -3,	 6,	   7,	  -2,	-1,	 0,	   -3,	  0,	0,	  0,   0,	0,	 0,	  0,   0,	0,	  0,	0,	  0,
		 0,		 0,		0,	  0,	 0,	   0,	  0,	0,	 0,	   0,	  0,	0,	  0,   0,	0,	 0,	  0,   0,	0,	  0,	0,	  0,
		 0,		 0,		0,	  0,	 0,	   0,	  0,	0,	 0,	   0,	  0,	0,	  0,   0,	0,	 0,	  0,   0,	0,	  0,	0,	  0,
		 0,		 0,		0,	  0,	 0,	   0,	  0,	0,	 0,	   0,	  0,	0,	  0,   0,	0,	 0,	  0,   0,	0,	  0},
	  },
	  {
		{"1960-01-01T00:00:00.000000Z"},
		ModelType::Dgrf,
		{-30421, -2169, 5791, -1555, 3002, -1967, 1590, 206, 1302, -1992, -414, 1289, 224, 878, -130, 957, 800, 135, 504,  -278, -394, 3,
		 269,	 -255,	-222, 362,	 16,   242,	  125,	-26, -117, -156,  -114, -63,  81,  46,	58,	  -10, 1,	99,	 -237, 60,	 -1,   -20,
		 -2,	 -11,	-113, -17,	 67,   -56,	  -55,	5,	 -28,  15,	  -6,	-32,  7,   -7,	23,	  17,  -18, 8,	 -17,  15,	 6,	   11,
		 -4,	 -14,	-11,  7,	 2,	   -18,	  10,	4,	 -5,   23,	  10,	1,	  8,   -20, 4,	  6,   -18, 0,	 12,   -9,	 2,	   1,
		 0,		 4,		-3,	  -1,	 9,	   -2,	  8,	3,	 0,	   -1,	  5,	1,	  -3,  4,	4,	  1,   0,	0,	 -1,   2,	 4,	   -5,
		 6,		 1,		1,	  -1,	 -1,   6,	  2,	0,	 0,	   -7,	  0,	0,	  0,   0,	0,	  0,   0,	0,	 0,	   0,	 0,	   0,
		 0,		 0,		0,	  0,	 0,	   0,	  0,	0,	 0,	   0,	  0,	0,	  0,   0,	0,	  0,   0,	0,	 0,	   0,	 0,	   0,
		 0,		 0,		0,	  0,	 0,	   0,	  0,	0,	 0,	   0,	  0,	0,	  0,   0,	0,	  0,   0,	0,	 0,	   0,	 0,	   0,
		 0,		 0,		0,	  0,	 0,	   0,	  0,	0,	 0,	   0,	  0,	0,	  0,   0,	0,	  0,   0,	0,	 0,	   0},
	  },
	  {
		{"1965-01-01T00:00:00.000000Z"},
		ModelType::Dgrf,
		{-30334, -2119, 5776, -1662, 2997, -2016, 1594, 114, 1297, -2038, -404, 1292, 240, 856, -165, 957, 804, 148, 479,  -269, -390, 13,
		 252,	 -269,	-219, 358,	 19,   254,	  128,	-31, -126, -157,  -97,	-62,  81,  45,	61,	  -11, 8,	100, -228, 68,	 4,	   -32,
		 1,		 -8,	-111, -7,	 75,   -57,	  -61,	4,	 -27,  13,	  -2,	-26,  6,   -6,	26,	  13,  -23, 1,	 -12,  13,	 5,	   7,
		 -4,	 -12,	-14,  9,	 0,	   -16,	  8,	4,	 -1,   24,	  11,	-3,	  4,   -17, 8,	  10,  -22, 2,	 15,   -13,	 7,	   10,
		 -4,	 -1,	-5,	  -1,	 10,   5,	  10,	1,	 -4,   -2,	  1,	-2,	  -3,  2,	2,	  1,   -5,	2,	 -2,   6,	 4,	   -4,
		 4,		 0,		0,	  -2,	 2,	   3,	  2,	0,	 0,	   -6,	  0,	0,	  0,   0,	0,	  0,   0,	0,	 0,	   0,	 0,	   0,
		 0,		 0,		0,	  0,	 0,	   0,	  0,	0,	 0,	   0,	  0,	0,	  0,   0,	0,	  0,   0,	0,	 0,	   0,	 0,	   0,
		 0,		 0,		0,	  0,	 0,	   0,	  0,	0,	 0,	   0,	  0,	0,	  0,   0,	0,	  0,   0,	0,	 0,	   0,	 0,	   0,
		 0,		 0,		0,	  0,	 0,	   0,	  0,	0,	 0,	   0,	  0,	0,	  0,   0,	0,	  0,   0,	0,	 0,	   0},
	  },
	  {
		{"1970-01-01T00:00:00.000000Z"},
		ModelType::Dgrf,
		{-30220, -2068, 5737, -1781, 3000, -2047, 1611, 25,	 1287, -2091, -366, 1278, 251, 838, -196, 952, 800, 167, 461,  -266, -395, 26,
		 234,	 -279,	-216, 359,	 26,   262,	  139,	-42, -139, -160,  -91,	-56,  83,  43,	64,	  -12, 15,	100, -212, 72,	 2,	   -37,
		 3,		 -6,	-112, 1,	 72,   -57,	  -70,	1,	 -27,  14,	  -4,	-22,  8,   -2,	23,	  13,  -23, -2,	 -11,  14,	 6,	   7,
		 -2,	 -15,	-13,  6,	 -3,   -17,	  5,	6,	 0,	   21,	  11,	-6,	  3,   -16, 8,	  10,  -21, 2,	 16,   -12,	 6,	   10,
		 -4,	 -1,	-5,	  0,	 10,   3,	  11,	1,	 -2,   -1,	  1,	-3,	  -3,  1,	2,	  1,   -5,	3,	 -1,   4,	 6,	   -4,
		 4,		 0,		1,	  -1,	 0,	   3,	  3,	1,	 -1,   -4,	  0,	0,	  0,   0,	0,	  0,   0,	0,	 0,	   0,	 0,	   0,
		 0,		 0,		0,	  0,	 0,	   0,	  0,	0,	 0,	   0,	  0,	0,	  0,   0,	0,	  0,   0,	0,	 0,	   0,	 0,	   0,
		 0,		 0,		0,	  0,	 0,	   0,	  0,	0,	 0,	   0,	  0,	0,	  0,   0,	0,	  0,   0,	0,	 0,	   0,	 0,	   0,
		 0,		 0,		0,	  0,	 0,	   0,	  0,	0,	 0,	   0,	  0,	0,	  0,   0,	0,	  0,   0,	0,	 0,	   0},
	  },
	  {
		{"1975-01-01T00:00:00.000000Z"},
		ModelType::Dgrf,
		{-30100, -2013, 5675, -1902, 3010, -2067, 1632, -68, 1276, -2144, -333, 1260, 262, 830, -223, 946, 791, 191, 438,  -265, -405, 39,
		 216,	 -288,	-218, 356,	 31,   264,	  148,	-59, -152, -159,  -83,	-49,  88,  45,	66,	  -13, 28,	99,	 -198, 75,	 1,	   -41,
		 6,		 -4,	-111, 11,	 71,   -56,	  -77,	1,	 -26,  16,	  -5,	-14,  10,  0,	22,	  12,  -23, -5,	 -12,  14,	 6,	   6,
		 -1,	 -16,	-12,  4,	 -8,   -19,	  4,	6,	 0,	   18,	  10,	-10,  1,   -17, 7,	  10,  -21, 2,	 16,   -12,	 7,	   10,
		 -4,	 -1,	-5,	  -1,	 10,   4,	  11,	1,	 -3,   -2,	  1,	-3,	  -3,  1,	2,	  1,   -5,	3,	 -2,   4,	 5,	   -4,
		 4,		 -1,	1,	  -1,	 0,	   3,	  3,	1,	 -1,   -5,	  0,	0,	  0,   0,	0,	  0,   0,	0,	 0,	   0,	 0,	   0,
		 0,		 0,		0,	  0,	 0,	   0,	  0,	0,	 0,	   0,	  0,	0,	  0,   0,	0,	  0,   0,	0,	 0,	   0,	 0,	   0,
		 0,		 0,		0,	  0,	 0,	   0,	  0,	0,	 0,	   0,	  0,	0,	  0,   0,	0,	  0,   0,	0,	 0,	   0,	 0,	   0,
		 0,		 0,		0,	  0,	 0,	   0,	  0,	0,	 0,	   0,	  0,	0,	  0,   0,	0,	  0,   0,	0,	 0,	   0},
	  },
	  {
		{"1980-01-01T00:00:00.000000Z"},
		ModelType::Dgrf,
		{-29992, -1956, 5604, -1997, 3027, -2129, 1663, -200, 1281, -2180, -336, 1251, 271, 833, -252, 938, 782, 212, 398,	-257, -419, 53,
		 199,	 -297,	-218, 357,	 46,   261,	  150,	-74,  -151, -162,  -78,	 -48,  92,	48,	 66,   -15, 42,	 93,  -192, 71,	  4,	-43,
		 14,	 -2,	-108, 17,	 72,   -59,	  -82,	2,	  -27,	21,	   -5,	 -12,  16,	1,	 18,   11,	-23, -2,  -10,	18,	  6,	7,
		 0,		 -18,	-11,  4,	 -7,   -22,	  4,	9,	  3,	16,	   6,	 -13,  -1,	-15, 5,	   10,	-21, 1,	  16,	-12,  9,	9,
		 -5,	 -3,	-6,	  -1,	 9,	   7,	  10,	2,	  -6,	-5,	   2,	 -4,   -4,	1,	 2,	   0,	-5,	 3,	  -2,	6,	  5,	-4,
		 3,		 0,		1,	  -1,	 2,	   4,	  3,	0,	  0,	-6,	   0,	 0,	   0,	0,	 0,	   0,	0,	 0,	  0,	0,	  0,	0,
		 0,		 0,		0,	  0,	 0,	   0,	  0,	0,	  0,	0,	   0,	 0,	   0,	0,	 0,	   0,	0,	 0,	  0,	0,	  0,	0,
		 0,		 0,		0,	  0,	 0,	   0,	  0,	0,	  0,	0,	   0,	 0,	   0,	0,	 0,	   0,	0,	 0,	  0,	0,	  0,	0,
		 0,		 0,		0,	  0,	 0,	   0,	  0,	0,	  0,	0,	   0,	 0,	   0,	0,	 0,	   0,	0,	 0,	  0,	0},
	  },
	  {
		{"1985-01-01T00:00:00.000000Z"},
		ModelType::Dgrf,
		{-29873, -1905, 5500, -2072, 3044, -2197, 1687, -306, 1296, -2208, -310, 1247, 284, 829, -297, 936, 780, 232, 361,	-249, -424, 69,
		 170,	 -297,	-214, 355,	 47,   253,	  150,	-93,  -154, -164,  -75,	 -46,  95,	53,	 65,   -16, 51,	 88,  -185, 69,	  4,	-48,
		 16,	 -1,	-102, 21,	 74,   -62,	  -83,	3,	  -27,	24,	   -2,	 -6,   20,	4,	 17,   10,	-23, 0,	  -7,	21,	  6,	8,
		 0,		 -19,	-11,  5,	 -9,   -23,	  4,	11,	  4,	14,	   4,	 -15,  -4,	-11, 5,	   10,	-21, 1,	  15,	-12,  9,	9,
		 -6,	 -3,	-6,	  -1,	 9,	   7,	  9,	1,	  -7,	-5,	   2,	 -4,   -4,	1,	 3,	   0,	-5,	 3,	  -2,	6,	  5,	-4,
		 3,		 0,		1,	  -1,	 2,	   4,	  3,	0,	  0,	-6,	   0,	 0,	   0,	0,	 0,	   0,	0,	 0,	  0,	0,	  0,	0,
		 0,		 0,		0,	  0,	 0,	   0,	  0,	0,	  0,	0,	   0,	 0,	   0,	0,	 0,	   0,	0,	 0,	  0,	0,	  0,	0,
		 0,		 0,		0,	  0,	 0,	   0,	  0,	0,	  0,	0,	   0,	 0,	   0,	0,	 0,	   0,	0,	 0,	  0,	0,	  0,	0,
		 0,		 0,		0,	  0,	 0,	   0,	  0,	0,	  0,	0,	   0,	 0,	   0,	0,	 0,	   0,	0,	 0,	  0,	0},
	  },
	  {
		{"1990-01-01T00:00:00.000000Z"},
		ModelType::Dgrf,
		{-29775, -1848, 5406, -2131, 3059, -2279, 1686, -373, 1314, -2239, -284, 1248, 293, 802, -352, 939, 780, 247, 325,	-240, -423, 84,
		 141,	 -299,	-214, 353,	 46,   245,	  154,	-109, -153, -165,  -69,	 -36,  97,	61,	 65,   -16, 59,	 82,  -178, 69,	  3,	-52,
		 18,	 1,		-96,  24,	 77,   -64,	  -80,	2,	  -26,	26,	   0,	 -1,   21,	5,	 17,   9,	-23, 0,	  -4,	23,	  5,	10,
		 -1,	 -19,	-10,  6,	 -12,  -22,	  3,	12,	  4,	12,	   2,	 -16,  -6,	-10, 4,	   9,	-20, 1,	  15,	-12,  11,	9,
		 -7,	 -4,	-7,	  -2,	 9,	   7,	  8,	1,	  -7,	-6,	   2,	 -3,   -4,	2,	 2,	   1,	-5,	 3,	  -2,	6,	  4,	-4,
		 3,		 0,		1,	  -2,	 3,	   3,	  3,	-1,	  0,	-6,	   0,	 0,	   0,	0,	 0,	   0,	0,	 0,	  0,	0,	  0,	0,
		 0,		 0,		0,	  0,	 0,	   0,	  0,	0,	  0,	0,	   0,	 0,	   0,	0,	 0,	   0,	0,	 0,	  0,	0,	  0,	0,
		 0,		 0,		0,	  0,	 0,	   0,	  0,	0,	  0,	0,	   0,	 0,	   0,	0,	 0,	   0,	0,	 0,	  0,	0,	  0,	0,
		 0,		 0,		0,	  0,	 0,	   0,	  0,	0,	  0,	0,	   0,	 0,	   0,	0,	 0,	   0,	0,	 0,	  0,	0},
	  },
	  {
		{"1995-01-01T00:00:00.000000Z"},
		ModelType::Dgrf,
		{-29692, -1784, 5306, -2200, 3070, -2366, 1681, -413, 1335, -2267, -262, 1249, 302, 759, -427, 940, 780, 262, 290,	-236, -418, 97,
		 122,	 -306,	-214, 352,	 46,   235,	  165,	-118, -143, -166,  -55,	 -17,  107, 68,	 67,   -17, 68,	 72,  -170, 67,	  -1,	-58,
		 19,	 1,		-93,  36,	 77,   -72,	  -69,	1,	  -25,	28,	   4,	 5,	   24,	4,	 17,   8,	-24, -2,  -6,	25,	  6,	11,
		 -6,	 -21,	-9,	  8,	 -14,  -23,	  9,	15,	  6,	11,	   -5,	 -16,  -7,	-4,	 4,	   9,	-20, 3,	  15,	-10,  12,	8,
		 -6,	 -8,	-8,	  -1,	 8,	   10,	  5,	-2,	  -8,	-8,	   3,	 -3,   -6,	1,	 2,	   0,	-4,	 4,	  -1,	5,	  4,	-5,
		 2,		 -1,	2,	  -2,	 5,	   1,	  1,	-2,	  0,	-7,	   0,	 0,	   0,	0,	 0,	   0,	0,	 0,	  0,	0,	  0,	0,
		 0,		 0,		0,	  0,	 0,	   0,	  0,	0,	  0,	0,	   0,	 0,	   0,	0,	 0,	   0,	0,	 0,	  0,	0,	  0,	0,
		 0,		 0,		0,	  0,	 0,	   0,	  0,	0,	  0,	0,	   0,	 0,	   0,	0,	 0,	   0,	0,	 0,	  0,	0,	  0,	0,
		 0,		 0,		0,	  0,	 0,	   0,	  0,	0,	  0,	0,	   0,	 0,	   0,	0,	 0,	   0,	0,	 0,	  0,	0},
	  },
	  {
		{"2000-01-01T00:00:00.000000Z"},
		ModelType::Dgrf,
		{-29619.4, -1728.2, 5186.1, -2267.7, 3068.4, -2481.6, 1670.9, -458,	  1339.6, -2288, -227.6, 1252.1, 293.4, 714.5,	-491.1, 932.3,
		 786.8,	   272.6,	250,	-231.9,	 -403,	 119.8,	  111.3,  -303.8, -218.8, 351.4, 43.8,	 222.3,	 171.9, -130.4, -133.1, -168.6,
		 -39.3,	   -12.9,	106.3,	72.3,	 68.2,	 -17.4,	  74.2,	  63.7,	  -160.9, 65.1,	 -5.9,	 -61.2,	 16.9,	0.7,	-90.4,	43.8,
		 79,	   -74,		-64.6,	0,		 -24.2,	 33.3,	  6.2,	  9.1,	  24,	  6.9,	 14.8,	 7.3,	 -25.4, -1.2,	-5.8,	24.4,
		 6.6,	   11.9,	-9.2,	-21.5,	 -7.9,	 8.5,	  -16.6,  -21.5,  9.1,	  15.5,	 7,		 8.9,	 -7.9,	-14.9,	-7,		-2.1,
		 5,		   9.4,		-19.7,	3,		 13.4,	 -8.4,	  12.5,	  6.3,	  -6.2,	  -8.9,	 -8.4,	 -1.5,	 8.4,	9.3,	3.8,	-4.3,
		 -8.2,	   -8.2,	4.8,	-2.6,	 -6,	 1.7,	  1.7,	  0,	  -3.1,	  4,	 -0.5,	 4.9,	 3.7,	-5.9,	1,		-1.2,
		 2,		   -2.9,	4.2,	0.2,	 0.3,	 -2.2,	  -1.1,	  -7.4,	  2.7,	  -1.7,	 0.1,	 -1.9,	 1.3,	1.5,	-0.9,	-0.1,
		 -2.6,	   0.1,		0.9,	-0.7,	 -0.7,	 0.7,	  -2.8,	  1.7,	  -0.9,	  0.1,	 -1.2,	 1.2,	 -1.9,	4,		-0.9,	-2.2,
		 -0.3,	   -0.4,	0.2,	0.3,	 0.9,	 2.5,	  -0.2,	  -2.6,	  0.9,	  0.7,	 -0.5,	 0.3,	 0.3,	0,		-0.3,	0,
		 -0.4,	   0.3,		-0.1,	-0.9,	 -0.2,	 -0.4,	  -0.4,	  0.8,	  -0.2,	  -0.9,	 -0.9,	 0.3,	 0.2,	0.1,	1.8,	-0.4,
		 -0.4,	   1.3,		-1,		-0.4,	 -0.1,	 0.7,	  0.7,	  -0.4,	  0.3,	  0.3,	 0.6,	 -0.1,	 0.3,	0.4,	-0.2,	0,
		 -0.5,	   0.1,		-0.9,	0},
	  },
	  {
		{"2005-01-01T00:00:00.000000Z"},
		ModelType::Dgrf,
		{-29554.6, -1669.05, 5077.99, -2337.24, 3047.69, -2594.5, 1657.76, -515.43, 1336.3, -2305.83, -198.86, 1246.39, 269.72,	 672.51,
		 -524.72,  920.55,	 797.96,  282.07,	210.65,	 -225.23, -379.86, 145.15,	100,	-305.36,  -227,	   354.41,	42.72,	 208.95,
		 180.25,   -136.54,	 -123.45, -168.05,	-19.57,	 -13.55,  103.85,  73.6,	69.56,	-20.33,	  76.74,   54.75,	-151.34, 63.63,
		 -14.58,   -63.53,	 14.58,	  0.24,		-86.36,	 50.94,	  79.88,   -74.46,	-61.14, -1.65,	  -22.57,  38.73,	6.82,	 12.3,
		 25.35,	   9.37,	 10.93,	  5.42,		-26.32,	 1.94,	  -4.64,   24.8,	7.62,	11.2,	  -11.73,  -20.88,	-6.88,	 9.83,
		 -18.11,   -19.71,	 10.17,	  16.22,	9.36,	 7.61,	  -11.25,  -12.76,	-4.87,	-0.06,	  5.58,	   9.76,	-20.11,	 3.58,
		 12.69,	   -6.94,	 12.67,	  5.01,		-6.72,	 -10.76,  -8.16,   -1.25,	8.1,	8.76,	  2.92,	   -6.66,	-7.73,	 -9.22,
		 6.01,	   -2.17,	 -6.12,	  2.19,		1.42,	 0.1,	  -2.35,   4.46,	-0.15,	4.76,	  3.06,	   -6.58,	0.29,	 -1.01,
		 2.06,	   -3.47,	 3.77,	  -0.86,	-0.21,	 -2.31,	  -2.09,   -7.93,	2.95,	-1.6,	  0.26,	   -1.88,	1.44,	 1.44,
		 -0.77,	   -0.31,	 -2.27,	  0.29,		0.9,	 -0.79,	  -0.58,   0.53,	-2.69,	1.8,	  -1.08,   0.16,	-1.58,	 0.96,
		 -1.9,	   3.99,	 -1.39,	  -2.15,	-0.29,	 -0.55,	  0.21,	   0.23,	0.89,	2.38,	  -0.38,   -2.63,	0.96,	 0.61,
		 -0.3,	   0.4,		 0.46,	  0.01,		-0.35,	 0.02,	  -0.36,   0.28,	0.08,	-0.87,	  -0.49,   -0.34,	-0.08,	 0.88,
		 -0.16,	   -0.88,	 -0.76,	  0.3,		0.33,	 0.28,	  1.72,	   -0.43,	-0.54,	1.18,	  -1.07,   -0.37,	-0.04,	 0.75,
		 0.63,	   -0.26,	 0.21,	  0.35,		0.53,	 -0.05,	  0.38,	   0.41,	-0.22,	-0.1,	  -0.57,   -0.18,	-0.82,	 0},
	  },
	  {
		{"2010-01-01T00:00:00.000000Z"},
		ModelType::Dgrf,
		{-29496.6, -1586.42, 4944.26, -2396.06, 3026.34, -2708.54, 1668.17, -575.73, 1339.85, -2326.54, -160.4,	 1232.1, 251.75, 633.73,
		 -537.03,  912.66,	 808.97,  286.48,	166.58,	 -211.03,  -356.83, 164.46,	 89.4,	  -309.72,	-230.87, 357.29, 44.58,	 200.26,
		 189.01,   -141.05,	 -118.06, -163.17,	-0.01,	 -8.03,	   101.04,	72.78,	 68.69,	  -20.9,	75.92,	 44.18,	 -141.4, 61.54,
		 -22.83,   -66.26,	 13.1,	  3.02,		-78.09,	 55.4,	   80.44,	-75,	 -57.8,	  -4.55,	-21.2,	 45.24,	 6.54,	 14,
		 24.96,	   10.46,	 7.03,	  1.64,		-27.61,	 4.92,	   -3.28,	24.41,	 8.21,	  10.84,	-14.5,	 -20.03, -5.59,	 11.83,
		 -19.34,   -17.41,	 11.61,	  16.71,	10.85,	 6.96,	   -14.05,	-10.74,	 -3.54,	  1.64,		5.5,	 9.45,	 -20.54, 3.45,
		 11.51,	   -5.27,	 12.75,	  3.13,		-7.14,	 -12.38,   -7.42,	-0.76,	 7.97,	  8.43,		2.14,	 -8.42,	 -6.08,	 -10.08,
		 7.01,	   -1.94,	 -6.24,	  2.73,		0.89,	 -0.1,	   -1.07,	4.71,	 -0.16,	  4.44,		2.45,	 -7.22,	 -0.33,	 -0.96,
		 2.13,	   -3.95,	 3.09,	  -1.99,	-1.03,	 -1.97,	   -2.8,	-8.31,	 3.05,	  -1.48,	0.13,	 -2.03,	 1.67,	 1.65,
		 -0.66,	   -0.51,	 -1.76,	  0.54,		0.85,	 -0.79,	   -0.39,	0.37,	 -2.51,	  1.79,		-1.27,	 0.12,	 -2.11,	 0.75,
		 -1.94,	   3.75,	 -1.86,	  -2.12,	-0.21,	 -0.87,	   0.3,		0.27,	 1.04,	  2.13,		-0.63,	 -2.49,	 0.95,	 0.49,
		 -0.11,	   0.59,	 0.52,	  0,		-0.39,	 0.13,	   -0.37,	0.27,	 0.21,	  -0.86,	-0.77,	 -0.23,	 0.04,	 0.87,
		 -0.09,	   -0.89,	 -0.87,	  0.31,		0.3,	 0.42,	   1.66,	-0.45,	 -0.59,	  1.08,		-1.14,	 -0.31,	 -0.07,	 0.78,
		 0.54,	   -0.18,	 0.1,	  0.38,		0.49,	 0.02,	   0.44,	0.42,	 -0.25,	  -0.26,	-0.53,	 -0.26,	 -0.79,	 0},
	  },
	  {
		{"2015-01-01T00:00:00.000000Z"},
		ModelType::Dgrf,
		{-29441.5, -1501.77, 4795.99, -2445.88, 3012.2, -2845.41, 1676.35, -642.17, 1350.33, -2352.26, -115.29, 1225.85, 245.04,  581.69,
		 -538.7,   907.42,	 813.68,  283.54,	120.49, -188.43,  -334.85, 180.95,	70.38,	 -329.23,  -232.91, 360.14,	 46.98,	  192.35,
		 196.98,   -140.94,	 -119.14, -157.4,	15.98,	4.3,	  100.12,  69.55,	67.57,	 -20.61,   72.79,	33.3,	 -129.85, 58.74,
		 -28.93,   -66.64,	 13.14,	  7.35,		-70.85, 62.41,	  81.29,   -75.99,	-54.27,	 -6.79,	   -19.53,	51.82,	 5.59,	  15.07,
		 24.45,	   9.32,	 3.27,	  -2.88,	-27.5,	6.61,	  -2.32,   23.98,	8.89,	 10.04,	   -16.78,	-18.26,	 -3.16,	  13.18,
		 -20.56,   -14.6,	 13.33,	  16.16,	11.76,	5.69,	  -15.98,  -9.1,	-2.02,	 2.26,	   5.33,	8.83,	 -21.77,  3.02,
		 10.76,	   -3.22,	 11.74,	  0.67,		-6.74,	-13.2,	  -6.88,   -0.1,	7.79,	 8.68,	   1.04,	-9.06,	 -3.89,	  -10.54,
		 8.44,	   -2.01,	 -6.26,	  3.28,		0.17,	-0.4,	  0.55,	   4.55,	-0.55,	 4.4,	   1.7,		-7.92,	 -0.67,	  -0.61,
		 2.13,	   -4.16,	 2.33,	  -2.85,	-1.8,	-1.12,	  -3.59,   -8.72,	3,		 -1.4,	   0,		-2.3,	 2.11,	  2.08,
		 -0.6,	   -0.79,	 -1.05,	  0.58,		0.76,	-0.7,	  -0.2,	   0.14,	-2.12,	 1.7,	   -1.44,	-0.22,	 -2.57,	  0.44,
		 -2.01,	   3.49,	 -2.34,	  -2.09,	-0.16,	-1.08,	  0.46,	   0.37,	1.23,	 1.75,	   -0.89,	-2.19,	 0.85,	  0.27,
		 0.1,	   0.72,	 0.54,	  -0.09,	-0.37,	0.29,	  -0.43,   0.23,	0.22,	 -0.89,	   -0.94,	-0.16,	 -0.03,	  0.72,
		 -0.02,	   -0.92,	 -0.88,	  0.42,		0.49,	0.63,	  1.56,	   -0.42,	-0.5,	 0.96,	   -1.24,	-0.19,	 -0.1,	  0.81,
		 0.42,	   -0.13,	 -0.04,	  0.38,		0.48,	0.08,	  0.48,	   0.46,	-0.3,	 -0.35,	   -0.43,	-0.36,	 -0.71,	  0},
	  },
	  {
		{"2020-01-01T00:00:00.000000Z"},
		ModelType::Igrf,
		{-29404.8, -1450.9, 4652.5, -2499.6, 2982,	 -2991.6, 1677,	 -734.6, 1363.2, -2381.2, -82.1, 1236.2, 241.9, 525.7,	-543.4, 903,
		 809.5,	   281.9,	86.3,	-158.4,	 -309.4, 199.7,	  48,	 -349.7, -234.3, 363.2,	  47.7,	 187.8,	 208.3, -140.7, -121.2, -151.2,
		 32.3,	   13.5,	98.9,	66,		 65.5,	 -19.1,	  72.9,	 25.1,	 -121.5, 52.8,	  -36.2, -64.5,	 13.5,	8.9,	-64.7,	68.1,
		 80.6,	   -76.7,	-51.5,	-8.2,	 -16.9,	 56.5,	  2.2,	 15.8,	 23.5,	 6.4,	  -2.2,	 -7.2,	 -27.2, 9.8,	-1.8,	23.7,
		 9.7,	   8.4,		-17.6,	-15.3,	 -0.5,	 12.8,	  -21.1, -11.7,	 15.3,	 14.9,	  13.7,	 3.6,	 -16.5, -6.9,	-0.3,	2.8,
		 5,		   8.4,		-23.4,	2.9,	 11,	 -1.5,	  9.8,	 -1.1,	 -5.1,	 -13.2,	  -6.3,	 1.1,	 7.8,	8.8,	0.4,	-9.3,
		 -1.4,	   -11.9,	9.6,	-1.9,	 -6.2,	 3.4,	  -0.1,	 -0.2,	 1.7,	 3.6,	  -0.9,	 4.8,	 0.7,	-8.6,	-0.9,	-0.1,
		 1.9,	   -4.3,	1.4,	-3.4,	 -2.4,	 -0.1,	  -3.8,	 -8.8,	 3,		 -1.4,	  0,	 -2.5,	 2.5,	2.3,	-0.6,	-0.9,
		 -0.4,	   0.3,		0.6,	-0.7,	 -0.2,	 -0.1,	  -1.7,	 1.4,	 -1.6,	 -0.6,	  -3,	 0.2,	 -2,	3.1,	-2.6,	-2,
		 -0.1,	   -1.2,	0.5,	0.5,	 1.3,	 1.4,	  -1.2,	 -1.8,	 0.7,	 0.1,	  0.3,	 0.8,	 0.5,	-0.2,	-0.3,	0.6,
		 -0.5,	   0.2,		0.1,	-0.9,	 -1.1,	 0,		  -0.3,	 0.5,	 0.1,	 -0.9,	  -0.9,	 0.5,	 0.6,	0.7,	1.4,	-0.3,
		 -0.4,	   0.8,		-1.3,	0,		 -0.1,	 0.8,	  0.3,	 0,		 -0.1,	 0.4,	  0.5,	 0.1,	 0.5,	0.5,	-0.4,	-0.5,
		 -0.4,	   -0.4,	-0.6,	0},
	  },
	  {
		{"2025-01-01T00:00:00.000000Z"},
		ModelType::Sv,
		{5.7, 7.4,	-25.9, -11, -7,	  -30.2, -2.1, -22.4, 2.2,	-5.9, 6,	3.1, -1.1, -12,	 0.5,  -1.2, -1.6, -0.1, -5.9, 6.5,
		 5.2, 3.6,	-5.1,  -5,	-0.3, 0.5,	 0,	   -0.6,  2.5,	0.2,  -0.6, 1.3, 3,	   0.9,	 0.3,  -0.5, -0.3, 0,	 0.4,  -1.6,
		 1.3, -1.3, -1.4,  0.8, 0,	  0,	 0.9,  1,	  -0.1, -0.2, 0.6,	0,	 0.6,  0.7,	 -0.8, 0.1,	 -0.2, -0.5, -1.1, -0.8,
		 0.1, 0.8,	0.3,   0,	0.1,  -0.2,	 -0.1, 0.6,	  0.4,	-0.2, -0.1, 0.5, 0.4,  -0.3, 0.3,  -0.4, -0.1, 0.5,	 0.4,  0,
		 0,	  0,	0,	   0,	0,	  0,	 0,	   0,	  0,	0,	  0,	0,	 0,	   0,	 0,	   0,	 0,	   0,	 0,	   0,
		 0,	  0,	0,	   0,	0,	  0,	 0,	   0,	  0,	0,	  0,	0,	 0,	   0,	 0,	   0,	 0,	   0,	 0,	   0,
		 0,	  0,	0,	   0,	0,	  0,	 0,	   0,	  0,	0,	  0,	0,	 0,	   0,	 0,	   0,	 0,	   0,	 0,	   0,
		 0,	  0,	0,	   0,	0,	  0,	 0,	   0,	  0,	0,	  0,	0,	 0,	   0,	 0,	   0,	 0,	   0,	 0,	   0,
		 0,	  0,	0,	   0,	0,	  0,	 0,	   0,	  0,	0,	  0,	0,	 0,	   0,	 0,	   0,	 0,	   0,	 0,	   0,
		 0,	  0,	0,	   0,	0,	  0,	 0,	   0,	  0,	0,	  0,	0,	 0,	   0,	 0,	   0},
	  },
	}}

{}

GEOMAG_NAMESPACE_END