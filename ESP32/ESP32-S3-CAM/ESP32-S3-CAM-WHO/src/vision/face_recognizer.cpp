#include "vision/face_recognizer.h"

#include "dl_recognition_database.hpp"

#include <algorithm>

namespace {

constexpr int TOP_K = 1;

// Multiple faces are possible; the stock recognizer uses the largest box, and
// that is also the face most likely to belong to the person in front of the
// camera.
const dl::detect::result_t &largest_face(const std::list<dl::detect::result_t> &faces)
{
    return *std::max_element(faces.begin(), faces.end(),
                             [](const dl::detect::result_t &a, const dl::detect::result_t &b) {
                                 return a.box_area() < b.box_area();
                             });
}

} // namespace

FaceRecognizer::FaceRecognizer(const std::string &db_path, HumanFaceFeat::model_type_t model_type) :
    m_feat(model_type, true), m_db_path(db_path)
{
}

FaceRecognizer::~FaceRecognizer()
{
    delete m_db;
}

dl::recognition::DataBase *FaceRecognizer::database()
{
    if (!m_db) {
        // get_feat_len() forces the lazy model load and tells the database how
        // wide each stored vector is.
        m_db = new dl::recognition::DataBase(m_db_path, m_feat.get_feat_len());
    }
    return m_db;
}

std::vector<dl::recognition::result_t> FaceRecognizer::recognize(
    const dl::image::img_t &img, const std::list<dl::detect::result_t> &faces, float similarity_threshold)
{
    if (faces.empty()) {
        return {};
    }
    dl::TensorBase *feat = m_feat.run(img, largest_face(faces).keypoint);
    return database()->query_feat(feat, similarity_threshold, TOP_K);
}

esp_err_t FaceRecognizer::enroll(const dl::image::img_t &img, const std::list<dl::detect::result_t> &faces)
{
    if (faces.empty()) {
        return ESP_FAIL;
    }
    dl::TensorBase *feat = m_feat.run(img, largest_face(faces).keypoint);
    return database()->enroll_feat(feat);
}

esp_err_t FaceRecognizer::clear_all_feats()
{
    return database()->clear_all_feats();
}

int FaceRecognizer::get_num_feats()
{
    return database()->get_num_feats();
}
