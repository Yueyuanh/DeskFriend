#pragma once

#include "dl_image_define.hpp"
#include "dl_recognition_define.hpp"
#include "human_face_recognition.hpp"

#include <list>
#include <string>
#include <vector>

// Face feature extraction plus the on-flash feature database.
//
// The stock HumanFaceRecognizer hard-codes a 0.5 similarity threshold and has
// no setter, so a face that is not square to the lens never produces a match.
// This project exposes the threshold on the web page, therefore the query goes
// straight through dl::recognition::DataBase with a caller-supplied value.
// Enrollment, clearing and counting behave exactly like the stock class.
class FaceRecognizer {
public:
    explicit FaceRecognizer(const std::string &db_path,
                            HumanFaceFeat::model_type_t model_type = HumanFaceFeat::MFN_S8_V1);
    ~FaceRecognizer();

    FaceRecognizer(const FaceRecognizer &) = delete;
    FaceRecognizer &operator=(const FaceRecognizer &) = delete;

    // Returns at most one match. An empty vector means "no enrolled face is
    // similar enough" at the given threshold.
    std::vector<dl::recognition::result_t> recognize(const dl::image::img_t &img,
                                                     const std::list<dl::detect::result_t> &faces,
                                                     float similarity_threshold);
    esp_err_t enroll(const dl::image::img_t &img, const std::list<dl::detect::result_t> &faces);
    esp_err_t clear_all_feats();
    int get_num_feats();

private:
    // Created on first use so the model and the database are only loaded when
    // a face actually shows up.
    dl::recognition::DataBase *database();

    HumanFaceFeat m_feat;
    dl::recognition::DataBase *m_db = nullptr;
    std::string m_db_path;
};
