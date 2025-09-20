#version 460
// Test double attach shader (build 1 fragment shader from 2 glsl)
float sphere(vec3 p, float r);
float box(vec3 p, vec3 b);

// Infinite cylinders
float cylinderZ(vec3 p, float r);
float cylinderY(vec3 p, float r);

// Finite cylinders
float cylinderZ(vec3 p, vec2 h);
float cylinderY(vec3 p, vec2 h);

// Material Value struct fwd declare
struct Value {float d; int mat;};
struct Material {
    vec3  color;        // [0, 1/pi]    reflective color
    float roughness;    // [0, 7]       shininess
    vec3 emission;		// [0, inf]     light emitting surface if nonzero
    float metalness;    // [0.02, 0.05] for non-metals, [0.6, 0.9] for metals
};

Material colors[7];

// Start of generated GLSL code
float sdf(vec3 p) {
	float r0 = (p).y;
	float r1 = box(mat3(0.877583,0,0.479426,0,1,0,-0.479426,0,0.877583)*p - vec3(-0.542803,1,0.159263), vec3(0.9,0.9,0.9))-0.25;
	r0 = min(r0,r1); //union 1
	float r2 = box(mat3(-4.37114e-08,0,1,0,1,0,-1,0,-4.37114e-08)*p - vec3(-1.4,1,3.9), vec3(2.9,0.9,0.9))-0.25;
	r0 = min(r0,r2); //union 2
	float r3 = box(mat3(0.992445,0,-0.12269,0,1,0,0.12269,0,0.992445)*p - vec3(0,5,0), vec3(0.9,0.9,0.9))-0.25;
	r0 = min(r0,r3); //union 3
	float r4 = cylinderY(p - vec3(-1.78885,3,3.57771), vec2(0.9,2.9))-0.25;
	r0 = min(r0,r4); //union 4
	float r5 = box(p - vec3(2,3,0), vec3(2.9,0.9,0.9))-0.25;
	float r6 = cylinderZ(p - vec3(2,0.6,0), 2.6);
	r6 *= -1.; //invert
	r5 = max(r5,r6); //intersect 1
	r5 -= 0.25;
	r0 = min(r0,r5); //union 5
	float r7 = box(mat3(-0.447214,0,-0.894427,0,1,0,0.894427,0,-0.447214)*p - vec3(2,7.5,0), vec3(2.9,0.9,0.9))-0.25;
	float r8 = cylinderZ(mat3(-0.447214,0,-0.894427,0,1,0,0.894427,0,-0.447214)*p - vec3(2,5,0), vec2(3.3,0.9));
	r7 = max(r7,r8); //intersect 1
	r7 -= 0.25;
	r0 = min(r0,r7); //union 6
	float r9 = cylinderY(p - vec3(4,5,0), vec2(0.9,0.9))-0.25;
	r0 = min(r0,r9); //union 7
	float r10 = box(mat3(0.992445,0,-0.12269,0,1,0,0.12269,0,0.992445)*p - vec3(8.65663,3,0.743535), vec3(0.9,2.9,0.9))-0.25;
	r0 = min(r0,r10); //union 8
	float r11 = box(mat3(0.921061,0,-0.389418,0,1,0,0.389418,0,0.921061)*p - vec3(6.37631,6.75,-1.61016), vec3(2.9,0.4,0.9))-0.25;
	r0 = min(r0,r11); //union 9
	float r12 = box(mat3(0.913089,0.40776,0,-0.40776,0.913089,0,0,0,1)*p - vec3(5.35888,4.24727,4.036), vec3(2.9,0.4,0.9))-0.25;
	r0 = min(r0,r12); //union 10
	float r13 = sphere(p - vec3(9,8.5,2.7), 1.);
	r0 = min(r0,r13); //union 11
	float r14 = sphere(p - vec3(1.2,1,3.2), 1.);
	r0 = min(r0,r14); //union 12
	float r15 = box(mat3(-0.447214,0,-0.894427,0,1,0,0.894427,0,-0.447214)*p - vec3(4.91935,1,-3.57771), vec3(0.9,0.9,0.9))-0.25;
	r0 = min(r0,r15); //union 13
	return r0;
}

Material material(vec3 p) {
	Value r0 = Value((p).y, 0);
	Value r1 = Value(box(mat3(0.877583,0,0.479426,0,1,0,-0.479426,0,0.877583)*p - vec3(-0.542803,1,0.159263), vec3(0.9,0.9,0.9))-0.25, 1);
	if(r1.d < r0.d)r0 = r1; // union 1
	Value r2 = Value(box(mat3(-4.37114e-08,0,1,0,1,0,-1,0,-4.37114e-08)*p - vec3(-1.4,1,3.9), vec3(2.9,0.9,0.9))-0.25, 0);
	if(r2.d < r0.d)r0 = r2; // union 2
	Value r3 = Value(box(mat3(0.992445,0,-0.12269,0,1,0,0.12269,0,0.992445)*p - vec3(0,5,0), vec3(0.9,0.9,0.9))-0.25, 2);
	if(r3.d < r0.d)r0 = r3; // union 3
	Value r4 = Value(cylinderY(p - vec3(-1.78885,3,3.57771), vec2(0.9,2.9))-0.25, 0);
	if(r4.d < r0.d)r0 = r4; // union 4
	Value r5 = Value(box(p - vec3(2,3,0), vec3(2.9,0.9,0.9))-0.25, 1);
	Value r6 = Value(cylinderZ(p - vec3(2,0.6,0), 2.6), 1);
	r6.d *= -1.; //invert
	if(r6.d > r5.d)r5 = r6; // intersect 1
	r5.d -= 0.25;
	if(r5.d < r0.d)r0 = r5; // union 5
	Value r9 = Value(cylinderY(p - vec3(4,5,0), vec2(0.9,0.9))-0.25, 1);
	if(r9.d < r0.d)r0 = r9; // union 7
	Value r10 = Value(box(mat3(0.992445,0,-0.12269,0,1,0,0.12269,0,0.992445)*p - vec3(8.65663,3,0.743535), vec3(0.9,2.9,0.9))-0.25, 0);
	if(r10.d < r0.d)r0 = r10; // union 8
	Value r11 = Value(box(mat3(0.921061,0,-0.389418,0,1,0,0.389418,0,0.921061)*p - vec3(6.37631,6.75,-1.61016), vec3(2.9,0.4,0.9))-0.25, 2);
	if(r11.d < r0.d)r0 = r11; // union 9
	Value r12 = Value(box(mat3(0.913089,0.40776,0,-0.40776,0.913089,0,0,0,1)*p - vec3(5.35888,4.24727,4.036), vec3(2.9,0.4,0.9))-0.25, 0);
	if(r12.d < r0.d)r0 = r12; // union 10
	Value r13 = Value(sphere(p - vec3(9,8.5,2.7), 1.), 0);
	if(r13.d < r0.d)r0 = r13; // union 11
	Value r14 = Value(sphere(p - vec3(1.2,1,3.2), 1.), 0);
	if(r14.d < r0.d)r0 = r14; // union 12
	return colors[r0.mat];
}

//void main(){}

// End of generated GLSL code
