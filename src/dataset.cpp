#include "dataset.hpp"

#include <algorithm>
#include <string>

#include "io_util.hpp"

namespace pev {

using json::Json;

bool Scene::has_tag(const std::string& t) const {
    return std::find(tags.begin(), tags.end(), t) != tags.end();
}

// --- 3D box (de)serialization -----------------------------------------------

Json box3d_to_json(const Box3D& b) {
    Json j = Json::object();
    j["cx"] = b.cx;
    j["cy"] = b.cy;
    j["cz"] = b.cz;
    j["length"] = b.length;
    j["width"] = b.width;
    j["height"] = b.height;
    j["heading"] = b.heading;
    return j;
}

Box3D box3d_from_json(const Json& j) {
    Box3D b;
    b.cx = j.at("cx").as_number();
    b.cy = j.at("cy").as_number();
    b.cz = j.at("cz").as_number();
    b.length = j.at("length").as_number();
    b.width = j.at("width").as_number();
    b.height = j.at("height").as_number();
    b.heading = j.at("heading").as_number();
    return b;
}

// --- Ground truth -----------------------------------------------------------

Json to_json(const GroundTruth& gt) {
    Json root = Json::object();
    root["schema"] = "pev.ground_truth.v1";
    Json scenes = Json::array();
    for (const Scene& s : gt.scenes) {
        Json js = Json::object();
        js["id"] = s.id;
        Json jtags = Json::array();
        for (const std::string& t : s.tags) jtags.push_back(t);
        js["tags"] = jtags;
        Json jframes = Json::array();
        for (const GtFrame& f : s.frames) {
            Json jf = Json::object();
            jf["index"] = f.index;
            Json jobjs = Json::array();
            for (const GtObject& o : f.objects) {
                Json jo = Json::object();
                jo["type"] = to_string(o.type);
                jo["box"] = box3d_to_json(o.box);
                jo["num_points"] = o.num_points;
                jo["track_id"] = o.track_id;
                jobjs.push_back(jo);
            }
            jf["objects"] = jobjs;
            jframes.push_back(jf);
        }
        js["frames"] = jframes;
        scenes.push_back(js);
    }
    root["scenes"] = scenes;
    return root;
}

GroundTruth ground_truth_from_json(const Json& j) {
    GroundTruth gt;
    for (const Json& js : j.at("scenes").as_array()) {
        Scene s;
        s.id = js.at("id").as_string();
        for (const Json& jt : js.at("tags").as_array()) {
            s.tags.push_back(jt.as_string());
        }
        for (const Json& jf : js.at("frames").as_array()) {
            GtFrame f;
            f.index = static_cast<int>(jf.at("index").as_int());
            for (const Json& jo : jf.at("objects").as_array()) {
                GtObject o;
                o.type = object_type_from_string(jo.at("type").as_string());
                o.box = box3d_from_json(jo.at("box"));
                o.num_points = static_cast<int>(jo.at("num_points").as_int());
                o.track_id = static_cast<int>(jo.at("track_id").as_int());
                f.objects.push_back(o);
            }
            s.frames.push_back(f);
        }
        gt.scenes.push_back(s);
    }
    return gt;
}

// --- Model runs -------------------------------------------------------------

Json to_json(const Run& run) {
    Json root = Json::object();
    root["schema"] = "pev.run.v1";
    root["name"] = run.name;
    Json scenes = Json::array();
    for (const RunScene& rs : run.scenes) {
        Json js = Json::object();
        js["scene_id"] = rs.scene_id;
        Json jframes = Json::array();
        for (const RunFrame& f : rs.frames) {
            Json jf = Json::object();
            jf["index"] = f.index;
            jf["latency_ms"] = f.latency_ms;
            Json jdets = Json::array();
            for (const Detection& d : f.detections) {
                Json jd = Json::object();
                jd["type"] = to_string(d.type);
                jd["score"] = d.score;
                jd["box"] = box3d_to_json(d.box);
                jd["track_id"] = d.track_id;
                jdets.push_back(jd);
            }
            jf["detections"] = jdets;
            jframes.push_back(jf);
        }
        js["frames"] = jframes;
        scenes.push_back(js);
    }
    root["scenes"] = scenes;
    return root;
}

Run run_from_json(const Json& j) {
    Run run;
    run.name = j.at("name").as_string();
    for (const Json& js : j.at("scenes").as_array()) {
        RunScene rs;
        rs.scene_id = js.at("scene_id").as_string();
        for (const Json& jf : js.at("frames").as_array()) {
            RunFrame f;
            f.index = static_cast<int>(jf.at("index").as_int());
            f.latency_ms = jf.at("latency_ms").as_number();
            for (const Json& jd : jf.at("detections").as_array()) {
                Detection d;
                d.type = object_type_from_string(jd.at("type").as_string());
                d.score = jd.at("score").as_number();
                d.box = box3d_from_json(jd.at("box"));
                d.track_id = static_cast<int>(jd.at("track_id").as_int());
                f.detections.push_back(d);
            }
            rs.frames.push_back(f);
        }
        run.scenes.push_back(rs);
    }
    return run;
}

// --- On-disk layout + load/save ---------------------------------------------

namespace {
std::string trim_trailing_slash(const std::string& dir) {
    std::string out = dir;
    while (out.size() > 1 && out.back() == '/') {
        out.pop_back();
    }
    return out;
}
}  // namespace

std::string ground_truth_path(const std::string& data_dir) {
    return trim_trailing_slash(data_dir) + "/ground_truth.json";
}

std::string run_path(const std::string& data_dir, const std::string& run_name) {
    return trim_trailing_slash(data_dir) + "/runs/" + run_name + ".json";
}

void save_ground_truth(const std::string& data_dir, const GroundTruth& gt) {
    write_text_file(ground_truth_path(data_dir), to_json(gt).dump() + "\n");
}

GroundTruth load_ground_truth(const std::string& data_dir) {
    const std::string path = ground_truth_path(data_dir);
    const Json j = Json::parse(read_text_file(path));
    return ground_truth_from_json(j);
}

void save_run(const std::string& data_dir, const Run& run) {
    write_text_file(run_path(data_dir, run.name), to_json(run).dump() + "\n");
}

Run load_run(const std::string& data_dir, const std::string& run_name) {
    const std::string path = run_path(data_dir, run_name);
    const Json j = Json::parse(read_text_file(path));
    Run run = run_from_json(j);
    if (run.name.empty()) {
        run.name = run_name;
    }
    return run;
}

}  // namespace pev
