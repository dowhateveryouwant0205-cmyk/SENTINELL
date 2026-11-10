// jni_bridge.cpp — JNI layer: the only bridge between Kotlin and the C++ core
#include <jni.h>
#include <string>
#include <vector>
#include "sentinel/SimulationEngine.h"

using namespace sentinel;

static std::string jstr(JNIEnv* env, jstring s) {
    if (!s) return "";
    const char* c = env->GetStringUTFChars(s, nullptr);
    std::string out(c);
    env->ReleaseStringUTFChars(s, c);
    return out;
}
static jstring jstr2(JNIEnv* env, const std::string& s) {
    return env->NewStringUTF(s.c_str());
}

extern "C" {

JNIEXPORT jboolean JNICALL
Java_com_sentinel_app_NativeBridge_nativeLoadMap(JNIEnv* env, jobject, jstring json) {
    return SimulationEngine::instance().loadMapJson(jstr(env, json)) ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT void JNICALL
Java_com_sentinel_app_NativeBridge_nativeStart(JNIEnv*, jobject) {
    SimulationEngine::instance().start();
}
JNIEXPORT void JNICALL
Java_com_sentinel_app_NativeBridge_nativePause(JNIEnv*, jobject) {
    SimulationEngine::instance().pause();
}
JNIEXPORT void JNICALL
Java_com_sentinel_app_NativeBridge_nativeReset(JNIEnv*, jobject) {
    SimulationEngine::instance().resetRun();
}
JNIEXPORT void JNICALL
Java_com_sentinel_app_NativeBridge_nativeTick(JNIEnv*, jobject, jdouble dt) {
    SimulationEngine::instance().tick(dt);
}

JNIEXPORT void JNICALL
Java_com_sentinel_app_NativeBridge_nativeSetMode(JNIEnv*, jobject, jint mode) {
    SimulationEngine::instance().setMode(mode == 1 ? SimMode::SENTINEL : SimMode::BASELINE);
}
JNIEXPORT jint JNICALL
Java_com_sentinel_app_NativeBridge_nativeGetMode(JNIEnv*, jobject) {
    return SimulationEngine::instance().mode() == SimMode::SENTINEL ? 1 : 0;
}
JNIEXPORT void JNICALL
Java_com_sentinel_app_NativeBridge_nativeSetRouter(JNIEnv*, jobject, jint kind) {
    SimulationEngine::instance().setRouter(static_cast<RouterKind>(kind));
}

JNIEXPORT jdouble JNICALL
Java_com_sentinel_app_NativeBridge_nativeSpawnIncident(JNIEnv* env, jobject, jstring type, jint node) {
    return SimulationEngine::instance().spawnIncident(jstr(env, type), node);
}

// ---- Flat FloatArray snapshots for fast UI polling ----
// Nodes: [count][id,x,y,signal(0R/1Y/2G)]*
JNIEXPORT jfloatArray JNICALL
Java_com_sentinel_app_NativeBridge_nativeGetNodes(JNIEnv* env, jobject) {
    auto ns = SimulationEngine::instance().nodes();
    std::vector<float> buf;
    buf.push_back((float)ns.size());
    for (auto& n : ns) {
        buf.push_back((float)n.id);
        buf.push_back(n.pos.x);
        buf.push_back(n.pos.y);
        buf.push_back((float)n.signal); // 0 RED, 1 YELLOW, 2 GREEN
    }
    jfloatArray arr = env->NewFloatArray((jsize)buf.size());
    env->SetFloatArrayRegion(arr, 0, (jsize)buf.size(), buf.data());
    return arr;
}

// Edges: [count][from,to,traffic]*
JNIEXPORT jfloatArray JNICALL
Java_com_sentinel_app_NativeBridge_nativeGetEdges(JNIEnv* env, jobject) {
    auto es = SimulationEngine::instance().edges();
    std::vector<float> buf;
    buf.push_back((float)es.size());
    for (auto& e : es) {
        buf.push_back((float)e.from);
        buf.push_back((float)e.to);
        buf.push_back((float)e.traffic);
    }
    jfloatArray arr = env->NewFloatArray((jsize)buf.size());
    env->SetFloatArrayRegion(arr, 0, (jsize)buf.size(), buf.data());
    return arr;
}

// Vehicles: [count][id,typeIdx(0A/1F/2P/3H),x,y,status(0..3),pathLen,path...,incidentId]*
JNIEXPORT jfloatArray JNICALL
Java_com_sentinel_app_NativeBridge_nativeGetVehicles(JNIEnv* env, jobject) {
    auto vs = SimulationEngine::instance().vehicles();
    std::vector<float> buf;
    buf.push_back((float)vs.size());
    for (auto& v : vs) {
        int t = 0;
        if (v.type == "FireTruck") t = 1;
        else if (v.type == "PoliceCar") t = 2;
        else if (v.type == "HazmatUnit") t = 3;
        buf.push_back((float)v.id);
        buf.push_back((float)t);
        buf.push_back(v.pos.x);
        buf.push_back(v.pos.y);
        buf.push_back((float)v.status);
        buf.push_back((float)v.path.size());
        for (int p : v.path) buf.push_back((float)p);
        buf.push_back((float)v.incidentId);
    }
    jfloatArray arr = env->NewFloatArray((jsize)buf.size());
    env->SetFloatArrayRegion(arr, 0, (jsize)buf.size(), buf.data());
    return arr;
}

// Incidents: [count][id,node,typeIdx(0med/1fire/2acc/3haz),severity,active,assignedVehicle,responseMin]*
JNIEXPORT jfloatArray JNICALL
Java_com_sentinel_app_NativeBridge_nativeGetIncidents(JNIEnv* env, jobject) {
    auto is = SimulationEngine::instance().incidents();
    std::vector<float> buf;
    buf.push_back((float)is.size());
    for (auto& i : is) {
        int t = 0;
        if (i.type == "fire") t = 1;
        else if (i.type == "accident") t = 2;
        else if (i.type == "hazmat") t = 3;
        buf.push_back((float)i.id);
        buf.push_back((float)i.node);
        buf.push_back((float)t);
        buf.push_back((float)i.severity);
        buf.push_back(i.active ? 1.f : 0.f);
        buf.push_back((float)i.assignedVehicle);
        buf.push_back((float)i.responseTimeMin);
    }
    jfloatArray arr = env->NewFloatArray((jsize)buf.size());
    env->SetFloatArrayRegion(arr, 0, (jsize)buf.size(), buf.data());
    return arr;
}

// Metrics: [simMinute, avgResp, lastResp, active, resolved, deployed, available, signalChanges, mode, router]
JNIEXPORT jfloatArray JNICALL
Java_com_sentinel_app_NativeBridge_nativeGetMetrics(JNIEnv* env, jobject) {
    auto m = SimulationEngine::instance().metrics();
    float buf[10] = {
        (float)m.simMinute, (float)m.avgResponseMin, (float)m.lastResponseMin,
        (float)m.activeIncidents, (float)m.resolvedIncidents,
        (float)m.unitsDeployed, (float)m.unitsAvailable,
        (float)m.signalChanges, (float)(m.mode == SimMode::SENTINEL ? 1 : 0),
        (float)(m.routerName == "Dijkstra" ? 0 : m.routerName == "A*" ? 1 : 2)
    };
    jfloatArray arr = env->NewFloatArray(10);
    env->SetFloatArrayRegion(arr, 0, 10, buf);
    return arr;
}

JNIEXPORT jstring JNICALL
Java_com_sentinel_app_NativeBridge_nativeReportJson(JNIEnv* env, jobject) {
    return jstr2(env, SimulationEngine::instance().reportJson());
}
JNIEXPORT jstring JNICALL
Java_com_sentinel_app_NativeBridge_nativeMapJson(JNIEnv* env, jobject) {
    return jstr2(env, SimulationEngine::instance().mapJson());
}

JNIEXPORT void JNICALL
Java_com_sentinel_app_NativeBridge_nativeRunAb(JNIEnv*, jobject, jint count) {
    SimulationEngine::instance().runAbComparison(count);
}
JNIEXPORT jboolean JNICALL
Java_com_sentinel_app_NativeBridge_nativeHasAbResult(JNIEnv*, jobject) {
    return SimulationEngine::instance().hasAbResult() ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jint JNICALL
Java_com_sentinel_app_NativeBridge_nativeNodeCount(JNIEnv*, jobject) {
    auto ns = SimulationEngine::instance().nodes();
    return (jint)ns.size();
}

} // extern C
