// ============================================================
// ballistic_hit_probability.hpp -- 命中概率扩展 (注释/禁用形式)
//
// 用途:
//   在文档描述的确定性弹道解算器 (RK4 + 灵敏度 + Newton 拦截) 基础上,
//   额外输出"命中概率 P_hit"。文档本身只保证数值上 F=0, 不产生概率;
//   概率需要把物理/测量误差通过灵敏度传播到落点散布, 再对装甲板命中区积分。
//
// 数学管线 (对应分析):
//   脱靶量 e = S_ψ,p δψ + S_ϑ,p δϑ + S_v0,p δv0 + S_CD,p δCD + δp_target
//   落点协方差 Σ = Σ_θ S_θ S_θ^T σ_θ^2        (θ ∈ {ψ, ϑ, v0, CD} + 感知)
//   命中概率  P_hit = ∫_{命中区} N(e; 0, Σ) de
//
// 说明:
//   * 文档已算 S_ψ,p / S_ϑ,p (18 维状态里的 6 个灵敏度分量);
//   本文件把状态扩到 30 维, 新增 v0 / CD 两组灵敏度, 并给出协方差与 P_hit。
//   * 当前整个实现位于 #if 0 内, 不参与编译; 启用时把下面第 1 行 #if 0 改成 #if 1。
//   * 使用前必须标定 NoiseParams 里的各个 σ (见下), 这是概率的唯一输入来源。
//
// 待标定 (NoiseParams):
//   σ_yaw / σ_pitch : 云台指向误差 (rad),       可由伺服跟踪误差或实测弹着散布反推
//   σ_v0            : 初速散布 (m/s),           25m 场景最敏感, 建议优先实测摩擦轮转速波动
//   σ_CD            : 阻力系数不确定 (无量纲),  换弹/温度气压变化大时不可忽略, 否则可先取 0
//   σ_pos           : 目标感知定位误差 (m)
//
// 与现有 solver 的接口约定 (本仓库尚未实现, 此处按文档的 solve()/solveFrom() 假设):
//   求解器返回 (ψ, ϑ, τ) 以及积分到 τ 的扩展状态 X(τ); 本文件在其后追加两步:
//     1) computeMissCovariance() 组装 3D 落点协方差
//     2) projectToPlane() + hitProbXXX() 投影到装甲板平面并算 P_hit
// ============================================================

#if 0   // <<< 注释形式: 需要启用时改成 #if 1

#include <array>
#include <cmath>

namespace ballistic {

// ------------------------------------------------------------
// 最小向量 / 矩阵 (无第三方依赖; 若项目引入 Eigen, 可直接替换)
// ------------------------------------------------------------
struct Vec3 {
    double x = 0.0, y = 0.0, z = 0.0;
};

inline double dot(const Vec3& a, const Vec3& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}
inline double norm(const Vec3& v) {
    return std::sqrt(dot(v, v));
}
// 按索引取分量: at(v,0)=x, at(v,1)=y, at(v,2)=z
inline double at(const Vec3& v, int i) {
    return (i == 0) ? v.x : ((i == 1) ? v.y : v.z);
}
inline Vec3 operator+(const Vec3& a, const Vec3& b) {
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}
inline Vec3 operator-(const Vec3& a, const Vec3& b) {
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}
inline Vec3 operator*(double k, const Vec3& v) {
    return {k * v.x, k * v.y, k * v.z};
}

// 行主序 3x3 / 2x2
using Mat3 = std::array<double, 9>;
using Mat2 = std::array<double, 4>;

inline double& M3(Mat3& m, int r, int c) { return m[r * 3 + c]; }
inline double  M3(const Mat3& m, int r, int c) { return m[r * 3 + c]; }
inline double& M2(Mat2& m, int r, int c) { return m[r * 2 + c]; }
inline double  M2(const Mat2& m, int r, int c) { return m[r * 2 + c]; }

inline Mat3 identity3() { return {1, 0, 0, 0, 1, 0, 0, 0, 1}; }

inline Vec3 matvec(const Mat3& A, const Vec3& v) {
    return {
        M3(A, 0, 0) * v.x + M3(A, 0, 1) * v.y + M3(A, 0, 2) * v.z,
        M3(A, 1, 0) * v.x + M3(A, 1, 1) * v.y + M3(A, 1, 2) * v.z,
        M3(A, 2, 0) * v.x + M3(A, 2, 1) * v.y + M3(A, 2, 2) * v.z,
    };
}

// ------------------------------------------------------------
// 物理参数与误差源方差
// ------------------------------------------------------------
struct BallisticParams {
    double v0 = 28.0;      // 初速 (m/s)
    double m  = 0.0032;    // 弹丸质量 (kg), 17mm 弹约 3.2g
    double d  = 0.017;     // 弹丸直径 (m)
    double CD = 0.47;      // 阻力系数 (球体)
    double rho = 1.225;    // 空气密度 (kg/m^3)
    double g  = 9.81;      // 重力加速度
};

struct NoiseParams {
    double sigma_yaw   = 0.0;   // yaw 指向误差 (rad)
    double sigma_pitch = 0.0;   // pitch 指向误差 (rad)
    double sigma_v0    = 0.0;   // 初速散布 (m/s)
    double sigma_CD    = 0.0;   // 阻力系数不确定 (无量纲), 可选, 先取 0
    double sigma_pos   = 0.0;   // 感知定位误差 (m), 各向同性
};

// ------------------------------------------------------------
// 扩展状态布局 (18 -> 30 维)
//   索引        含义
//   0..2        p            位置
//   3..5        v            速度
//   6..8        S_ψ,p        ∂p/∂ψ
//   9..11       S_ψ,v        ∂v/∂ψ
//   12..14      S_ϑ,p        ∂p/∂ϑ
//   15..17      S_ϑ,v        ∂v/∂ϑ
//   18..20      S_v0,p       ∂p/∂v0        <-- 新增
//   21..23      S_v0,v       ∂v/∂v0        <-- 新增
//   24..26      S_CD,p       ∂p/∂CD        <-- 新增(可选)
//   27..29      S_CD,v       ∂v/∂CD        <-- 新增(可选)
// ------------------------------------------------------------
constexpr int kStateDim = 30;

constexpr int kP      = 0;
constexpr int kV      = 3;
constexpr int kSYawP  = 6;    // S_ψ,p
constexpr int kSYawV  = 9;    // S_ψ,v
constexpr int kSPitP  = 12;   // S_ϑ,p
constexpr int kSPitV  = 15;   // S_ϑ,v
constexpr int kSV0P   = 18;   // S_v0,p
constexpr int kSV0V   = 21;   // S_v0,v
constexpr int kSCDP   = 24;   // S_CD,p
constexpr int kSCDV   = 27;   // S_CD,v

using State = std::array<double, kStateDim>;

// ------------------------------------------------------------
// 物理模型 (与文档一致: f ∝ v^2)
// ------------------------------------------------------------
// c = ρ CD π d² / (8 m)
inline double dragCoeff(const BallisticParams& p) {
    return p.rho * p.CD * M_PI * p.d * p.d / (8.0 * p.m);
}

// a(v) = [ -c||v|| v_x, -c||v|| v_y, -g - c||v|| v_z ]
inline Vec3 accel(const Vec3& v, const BallisticParams& p) {
    const double c = dragCoeff(p);
    const double k = c * norm(v);           // k = c||v||
    return {-k * v.x, -k * v.y, -p.g - k * v.z};
}

// A(v) = ∂a/∂v = -c [ (v v^T)/||v|| + ||v|| I ]
inline Mat3 dAccel_dv(const Vec3& v, const BallisticParams& p) {
    const double c = dragCoeff(p);
    const double s = norm(v);
    const double k = -c / s;                // 注意: A 的第 1 项含 1/s
    Mat3 A{};
    M3(A, 0, 0) = k * (v.x * v.x) - c * s;
    M3(A, 0, 1) = k * (v.x * v.y);
    M3(A, 0, 2) = k * (v.x * v.z);
    M3(A, 1, 0) = M3(A, 0, 1);
    M3(A, 1, 1) = k * (v.y * v.y) - c * s;
    M3(A, 1, 2) = k * (v.y * v.z);
    M3(A, 2, 0) = M3(A, 0, 2);
    M3(A, 2, 1) = M3(A, 1, 2);
    M3(A, 2, 2) = k * (v.z * v.z) - c * s;
    return A;
}

// ∂a/∂CD = -(c/CD) ||v|| v   (c = CD · c_unit, 重力项与 CD 无关)
inline Vec3 dAccel_dCD(const Vec3& v, const BallisticParams& p) {
    const double c = dragCoeff(p);
    const double k = -(c / p.CD) * norm(v);
    return {k * v.x, k * v.y, k * v.z};
}

// ------------------------------------------------------------
// 扩展 ODE 右端: dX/dt = f(X)
// 灵敏度块通用规则:
//   d(S_θ,p)/dt = S_θ,v
//   d(S_θ,v)/dt = A(v)·S_θ,v + ∂a/∂θ
// 其中 ∂a/∂θ: 对 ψ/ϑ/v0 为 0, 对 CD 为 dAccel_dCD
// ------------------------------------------------------------
inline void sensitivityDerivative(const Mat3& A, const Vec3& S_v,
                                  const Vec3& forcing,
                                  Vec3& dS_p, Vec3& dS_v) {
    dS_p = S_v;
    dS_v = matvec(A, S_v) + forcing;
}

inline void extendedRhs(const State& X, State& dX, const BallisticParams& p) {
    const Vec3 v{X[kV], X[kV + 1], X[kV + 2]};
    const Vec3 a = accel(v, p);
    const Mat3 A = dAccel_dv(v, p);
    const Vec3 fCD = dAccel_dCD(v, p);
    const Vec3 zero{0, 0, 0};

    // dp/dt = v
    dX[kP] = v.x; dX[kP + 1] = v.y; dX[kP + 2] = v.z;
    // dv/dt = a
    dX[kV] = a.x; dX[kV + 1] = a.y; dX[kV + 2] = a.z;

    // 5 组灵敏度: (S_p 偏移, S_v 偏移, forcing)
    const int blkP[5] = {kSYawP, kSPitP, kSV0P, kSCDP, kSCDP};
    const int blkV[5] = {kSYawV, kSPitV, kSV0V, kSCDV, kSCDV};
    const Vec3 forcing[5] = {zero, zero, zero, fCD, fCD};
    for (int b = 0; b < 5; ++b) {
        const Vec3 Sv{X[blkV[b]], X[blkV[b] + 1], X[blkV[b] + 2]};
        Vec3 dSp, dSv;
        sensitivityDerivative(A, Sv, forcing[b], dSp, dSv);
        dX[blkP[b]] = dSp.x; dX[blkP[b] + 1] = dSp.y; dX[blkP[b] + 2] = dSp.z;
        dX[blkV[b]] = dSv.x; dX[blkV[b] + 1] = dSv.y; dX[blkV[b] + 2] = dSv.z;
    }
}

// ------------------------------------------------------------
// 灵敏度初值 (t=0)
//   S_ψ,p(0) = ∂p_M/∂ψ,  S_ψ,v(0) = v0 ∂d_M/∂ψ    (文档已给, 沿用)
//   S_ϑ,p(0) = ∂p_M/∂ϑ,  S_ϑ,v(0) = v0 ∂d_M/∂ϑ    (文档已给, 沿用)
//   新增:
//   S_v0,p(0) = 0            (位置不依赖 v0)
//   S_v0,v(0) = d_M          (∂v_M/∂v0 = 单位出射方向)
//   S_CD,p(0) = 0            (初值不依赖 CD)
//   S_CD,v(0) = 0            (初值不依赖 CD)
// ------------------------------------------------------------
// 说明: 此处 d_M 为枪口单位出射方向 (world 系), 由云台姿态和安装几何得到。
//       下面函数只补 v0/CD 两组, ψ/ϑ 初值由你现有代码填充后传入。
inline void initV0CDSensitivities(State& X, const Vec3& dM) {
    for (int i = 0; i < 3; ++i) {
        X[kSV0P + i] = 0.0;
        X[kSCDP + i] = 0.0;
        X[kSCDV + i] = 0.0;
    }
    X[kSV0V + 0] = dM.x;   // S_v0,v(0) = d_M
    X[kSV0V + 1] = dM.y;
    X[kSV0V + 2] = dM.z;
}

// ------------------------------------------------------------
// 落点协方差 (3D)
//   Σ_miss = Σ_θ S_θ,p S_θ,p^T σ_θ² + σ_pos² I
// ------------------------------------------------------------
inline Mat3 computeMissCovariance(const State& X_tau, const NoiseParams& n) {
    const Vec3 S_yaw {X_tau[kSYawP], X_tau[kSYawP + 1], X_tau[kSYawP + 2]};
    const Vec3 S_pit {X_tau[kSPitP], X_tau[kSPitP + 1], X_tau[kSPitP + 2]};
    const Vec3 S_v0  {X_tau[kSV0P],  X_tau[kSV0P + 1],  X_tau[kSV0P + 2]};
    const Vec3 S_cd  {X_tau[kSCDP],  X_tau[kSCDP + 1],  X_tau[kSCDP + 2]};

    Mat3 Sigma{};
    // 每一项: σ² * S S^T
    const struct { Vec3 s; double var; } terms[4] = {
        {S_yaw, n.sigma_yaw * n.sigma_yaw},
        {S_pit, n.sigma_pitch * n.sigma_pitch},
        {S_v0,  n.sigma_v0 * n.sigma_v0},
        {S_cd,  n.sigma_CD * n.sigma_CD},
    };
    for (const auto& t : terms) {
        for (int i = 0; i < 3; ++i) {
            for (int j = 0; j < 3; ++j) {
                M3(Sigma, i, j) += t.var * at(t.s, i) * at(t.s, j);
            }
        }
    }
    const double sp2 = n.sigma_pos * n.sigma_pos;
    M3(Sigma, 0, 0) += sp2;
    M3(Sigma, 1, 1) += sp2;
    M3(Sigma, 2, 2) += sp2;
    return Sigma;
}

// ------------------------------------------------------------
// 投影到装甲板平面 (3D -> 2D)
//   n: 装甲板平面法向 (单位向量), e1/e2 为其面内正交基
//   Σ2 = P Σ P^T, P 的行 = e1, e2
// ------------------------------------------------------------
inline Mat2 projectToPlane(const Mat3& Sigma, const Vec3& n) {
    // 用 Gram-Schmidt 由法向构造面内正交基 e1,e2
    Vec3 e1;
    if (std::abs(n.x) < 0.9) {
        e1 = {1, 0, 0};
    } else {
        e1 = {0, 1, 0};
    }
    // e1 -= (e1·n) n
    e1 = e1 - dot(e1, n) * n;
    e1 = (1.0 / norm(e1)) * e1;
    // e2 = n × e1
    Vec3 e2 = {n.y * e1.z - n.z * e1.y,
               n.z * e1.x - n.x * e1.z,
               n.x * e1.y - n.y * e1.x};

    // Σ2 = P Σ P^T, 2x3 * 3x3 * 3x2
    Mat2 S2{};
    const Vec3 row[2] = {e1, e2};
    for (int i = 0; i < 2; ++i) {
        for (int j = 0; j < 2; ++j) {
            double sum = 0.0;
            for (int a = 0; a < 3; ++a) {
                for (int b = 0; b < 3; ++b) {
                    sum += at(row[i], a) * M3(Sigma, a, b) * at(row[j], b);
                }
            }
            M2(S2, i, j) = sum;
        }
    }
    return S2;
}

// ------------------------------------------------------------
// 命中概率
// ------------------------------------------------------------
// 情形 1: 圆形命中区 + 各向同性散布 σ -> 瑞利闭式解
//   P_hit = 1 - exp(-R_hit² / (2σ²))
inline double hitProbRayleigh(double R_hit, double sigma) {
    if (sigma <= 0.0) return 1.0;                 // 无散布 -> 必中
    return 1.0 - std::exp(-R_hit * R_hit / (2.0 * sigma * sigma));
}

// 情形 2: 矩形装甲板(半宽 halfW, 半高 halfH) + 已对角化协方差(σx, σy) -> 闭式解
//   P_hit = [2Φ(halfW/σx) - 1] · [2Φ(halfH/σy) - 1]
//   注意: 仅当协方差主轴与装甲板边平行时严格成立, 否则按近似处理。
inline double hitProbRect(double halfW, double halfH, double sx, double sy) {
    auto Phi = [](double x) { return 0.5 * std::erfc(-x / std::sqrt(2.0)); };
    return (2.0 * Phi(halfW / sx) - 1.0) * (2.0 * Phi(halfH / sy) - 1.0);
}

// 情形 3: 一般各向异性高斯 + 圆形命中区 -> Monte Carlo (通用兜底)
//   对角化 Σ2 = L L^T (Cholesky), 采样 x = L z, 统计落在半径 R_hit 内的比例。
inline double hitProbMonteCarlo(const Mat2& Sigma2, double R_hit, int N = 100000) {
    // Cholesky: Σ = [a c; c b] = L L^T, L = [l0 0; l1 l2]
    const double a = M2(Sigma2, 0, 0);
    const double b = M2(Sigma2, 1, 1);
    const double c = M2(Sigma2, 0, 1);
    const double l0 = std::sqrt(a);
    const double l1 = c / l0;
    const double l2 = std::sqrt(std::max(0.0, b - l1 * l1));

    int hit = 0;
    for (int i = 0; i < N; ++i) {
        // Box-Muller 生成标准正态 z1,z2
        const double u1 = (rand() + 0.5) / (RAND_MAX + 1.0);
        const double u2 = (rand() + 0.5) / (RAND_MAX + 1.0);
        const double z1 = std::sqrt(-2.0 * std::log(u1)) * std::cos(2.0 * M_PI * u2);
        const double z2 = std::sqrt(-2.0 * std::log(u1)) * std::sin(2.0 * M_PI * u2);
        const double x = l0 * z1;
        const double y = l1 * z1 + l2 * z2;
        if (x * x + y * y <= R_hit * R_hit) ++hit;
    }
    return static_cast<double>(hit) / static_cast<double>(N);
}

// ------------------------------------------------------------
// 顶层: 求解 + 命中概率
//   假设你已有 solve()/solveFrom() 求出 (ψ,ϑ,τ) 及积分到 τ 的扩展状态 X(τ),
//   下面函数只负责"概率"这一半。
// ------------------------------------------------------------
struct HitSolution {
    double psi = 0.0, theta = 0.0, tau = 0.0;
    State  X_tau{};          // 积分到 τ 的 30 维扩展状态
    Mat3   Sigma_miss{};     // 3D 落点协方差
    Mat2   Sigma_plane{};    // 装甲板平面 2D 协方差
    double rms_miss = 0.0;   // RMS 脱靶量 = sqrt(λ1 + λ2)
    double P_hit = 0.0;      // 命中概率
};

// 使用步骤:
//   1) 在 solve()/solveFrom() 内部, 把 RK4 状态从 18 维换成 30 维 State,
//      并用 initV0CDSensitivities() 补上 v0/CD 初值;
//   2) 求解结束后拿到 X(τ) = X_tau;
//   3) 下面 computeHitProbability() 一步算出协方差与概率。
inline void computeHitProbability(HitSolution& s,
                                  const NoiseParams& n,
                                  const Vec3& plate_normal,
                                  double R_hit) {
    s.Sigma_miss  = computeMissCovariance(s.X_tau, n);
    s.Sigma_plane = projectToPlane(s.Sigma_miss, plate_normal);

    // RMS 脱靶 = sqrt(λ1 + λ2), 即 2D 协方差的迹
    const double trace2 = M2(s.Sigma_plane, 0, 0) + M2(s.Sigma_plane, 1, 1);
    s.rms_miss = std::sqrt(trace2);

    // 等价的各向同性 σ = sqrt(平均特征值) = sqrt(tr/2), 用于瑞利近似
    const double sigma_iso = std::sqrt(trace2 / 2.0);
    s.P_hit = hitProbRayleigh(R_hit, sigma_iso);
    // 需要更精确时改用:
    // s.P_hit = hitProbMonteCarlo(s.Sigma_plane, R_hit);
}

} // namespace ballistic

#endif // 0
