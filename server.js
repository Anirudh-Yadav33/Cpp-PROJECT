// ==========================================================================
// EvalAI Backend Server - Express.js
// ==========================================================================
const express = require('express');
const path = require('path');

const app = express();
const PORT = process.env.PORT || 3000;

app.use(express.json({ limit: '10mb' }));
app.use(express.static(path.join(__dirname, 'public')));

// ==========================================================================
// In-Memory Data Store (demo / prototype)
// ==========================================================================
const db = {
  users: [
    { id: 1, full_name: 'Dr. Alan Smith', username: 'prof_smith', password: 'pass123', role: 'faculty' },
    { id: 2, full_name: 'Alex Johnson', username: 'alex_student', password: 'pass123', role: 'student' },
  ],
  courses: [
    { id: 1, subject_name: 'Deep Learning & Computer Vision', faculty_username: 'prof_smith', faculty_name: 'Dr. Alan Smith', code: '84920' },
  ],
  enrollments: [
    { id: 1, course_id: 1, student_username: 'alex_student', student_name: 'Alex Johnson', code: '84920', subject_name: 'Deep Learning & Computer Vision', course_code: '84920', faculty_username: 'prof_smith', faculty_name: 'Dr. Alan Smith', status: 'accepted', requested_at: '2025-01-15' },
  ],
  submissions: [
    {
      id: 1,
      course_id: 1,
      student_username: 'alex_student',
      student_name: 'Alex Johnson',
      title: 'AI-Driven Medical Risk Predictor',
      description: 'A deep learning model that predicts patient readmission risk using electronic health records. The system uses a combination of LSTM networks and attention mechanisms to analyze temporal patient data and generate risk scores with explainability.',
      pdf_filename: 'Medical_Risk_Predictor_Report.pdf',
      pdf_content: `TITLE: AI-Driven Medical Risk Predictor
STUDENT: Alex Johnson
COURSE: Deep Learning & Computer Vision

ABSTRACT:
This project presents a deep learning system for predicting hospital readmission risk using Electronic Health Records (EHR). The model combines Long Short-Term Memory (LSTM) networks with a self-attention mechanism to capture temporal patterns in patient data.

CHAPTER 1: INTRODUCTION
Hospital readmissions cost the US healthcare system over $26 billion annually. Accurate prediction of readmission risk can help hospitals allocate resources effectively. Our system aims to outperform existing logistic regression and XGBoost baselines.

CHAPTER 2: LITERATURE REVIEW
Existing systems:
1. Rajkomar et al. (2018) - Deep learning for EHR prediction using FeatForge. Achieved AUC 0.75.
2. Harutyunyan et al. (2019) - Multitask learning with MIMIC-III. AUC 0.76.
3. Golas et al. (2018) - Deep learning for clinical pathways. AUC 0.72.

Our contribution: We add a multi-head attention mechanism on top of LSTM to improve interpretability and achieve AUC 0.82, surpassing all listed baselines.

CHAPTER 3: METHODOLOGY
- Data Preprocessing: Missing value imputation, normalization, sequence padding
- Model Architecture: Bidirectional LSTM (2 layers, 128 units) → Multi-Head Attention (4 heads) → Dense layers → Sigmoid output
- Training: Adam optimizer, lr=0.001, batch_size=64, 50 epochs with early stopping
- Evaluation Metrics: AUC-ROC, F1-score, Precision, Recall

CHAPTER 4: IMPLEMENTATION & CODE
Key code snippet:
\`\`\`python
import torch
import torch.nn as nn

class MedicalRiskPredictor(nn.Module):
    def __init__(self, input_dim, hidden_dim=128, num_heads=4):
        super().__init__()
        self.lstm = nn.LSTM(input_dim, hidden_dim, num_layers=2, batch_first=True, bidirectional=True)
        self.attention = nn.MultiheadAttention(hidden_dim * 2, num_heads, batch_first=True)
        self.classifier = nn.Sequential(
            nn.Linear(hidden_dim * 2, 64),
            nn.ReLU(),
            nn.Dropout(0.3),
            nn.Linear(64, 1),
            nn.Sigmoid()
        )
    
    def forward(self, x):
        lstm_out, _ = self.lstm(x)
        attn_out, weights = self.attention(lstm_out, lstm_out, lstm_out)
        out = self.classifier(attn_out[:, -1, :])
        return out, weights
\`\`\`

CHAPTER 5: RESULTS & OUTPUTS
- AUC-ROC: 0.82 (vs baseline 0.76)
- F1-Score: 0.78
- Precision: 0.81
- Recall: 0.75
- Confusion matrix and ROC curves included in appendix

CHAPTER 6: MODULES
Module 1: Data Pipeline (ETL, cleaning, feature engineering)
Module 2: Model Training (LSTM + Attention training loop)
Module 3: Prediction API (FastAPI endpoint for real-time inference)
Module 4: Dashboard (Streamlit-based visualization of predictions)
Module 5: Model Explainability (SHAP values for feature importance)

CHAPTER 7: CONCLUSION & FUTURE WORK
The system demonstrates that attention-augmented LSTM networks can effectively predict readmission risk. Future work includes integrating real-time vitals data and multi-hospital validation.`,
      special_code: '84920',
      submitted_at: '2025-01-20',
      status: 'graded',
      has_ai_eval: true,
      ai_eval: {
        r1: 26, r1_fb: 'Strong novelty — attention mechanism on LSTM for EHR is not a common approach in listed literature. Clear differentiation from Rajkomar and Harutyunyan baselines with +6% AUC improvement.',
        r2: 24, r2_fb: 'Clean PyTorch implementation with proper module structure. Code is well-organized with clear variable naming. Minor improvement possible in handling edge cases for missing data.',
        r3: 16, r3_fb: 'Good modular decomposition into 5 clear modules. Outputs include ROC curves and confusion matrix. Dashboard visualization adds practical value.',
        r4: 15, r4_fb: 'Well-structured report with proper chapters. Literature review compares against 3 existing systems. Scope is appropriate for the project. Minor: could include more ablation studies.',
        total: 81, is_pass: true, notes: 'AI evaluation based on mandatory rubric criteria analysis of full PDF content.'
      },
      has_faculty_eval: false,
      faculty_eval: null,
      has_final_eval: false,
      final_eval: null,
      chat_history: [
        { sender: 'AI', message: 'Hello! I have completed my initial AI evaluation of "AI-Driven Medical Risk Predictor" by Alex Johnson. The project scores 81/100 (PASS). I used attention-augmented LSTM for medical risk prediction which shows novelty. Feel free to ask me about any rubric area, my scoring rationale, or suggest adjustments you\'d like to discuss.' },
      ],
    },
  ],
  nextIds: { user: 3, course: 2, enrollment: 2, submission: 2 },
};

let nextUserId = 3;
let nextCourseId = 2;
let nextEnrollmentId = 2;
let nextSubmissionId = 2;

// ==========================================================================
// AUTH ROUTES
// ==========================================================================
app.post('/api/login', (req, res) => {
  const { username, password } = req.body;
  const user = db.users.find(u => u.username === username && u.password === password);
  if (user) {
    res.json({ success: true, user: { id: user.id, full_name: user.full_name, username: user.username, role: user.role } });
  } else {
    res.json({ success: false, message: 'Invalid username or password.' });
  }
});

app.post('/api/register', (req, res) => {
  const { full_name, username, password, role } = req.body;
  if (db.users.find(u => u.username === username)) {
    return res.json({ success: false, message: 'Username already taken.' });
  }
  const newUser = { id: nextUserId++, full_name, username, password, role };
  db.users.push(newUser);
  res.json({ success: true, message: 'Account created successfully! You can now log in.' });
});

// ==========================================================================
// COURSE ROUTES
// ==========================================================================
app.get('/api/courses/list', (req, res) => {
  res.json({ success: true, courses: db.courses.map(c => ({ code: c.code, subject_name: c.subject_name, faculty_name: c.faculty_name })) });
});

app.post('/api/courses/create', (req, res) => {
  const { subject_name, faculty_username, faculty_name, code } = req.body;
  if (db.courses.find(c => c.code === code)) {
    return res.json({ success: false, message: 'Course code already exists.' });
  }
  const course = { id: nextCourseId++, subject_name, faculty_username, faculty_name, code };
  db.courses.push(course);
  res.json({ success: true, message: `Course "${subject_name}" created with code ${code}.` });
});

app.post('/api/courses/enroll', (req, res) => {
  const { code, student_username, student_name } = req.body;
  const course = db.courses.find(c => c.code === code);
  if (!course) return res.json({ success: false, message: 'Invalid course code.' });

  const existing = db.enrollments.find(e => e.course_id === course.id && e.student_username === student_username);
  if (existing) return res.json({ success: false, message: 'You are already enrolled or have a pending request for this course.' });

  const enrollment = {
    id: nextEnrollmentId++,
    course_id: course.id,
    student_username,
    student_name,
    code,
    subject_name: course.subject_name,
    course_code: course.code,
    faculty_username: course.faculty_username,
    faculty_name: course.faculty_name,
    status: 'pending',
    requested_at: new Date().toISOString().split('T')[0],
  };
  db.enrollments.push(enrollment);
  res.json({ success: true, message: 'Enrollment request sent to faculty.' });
});

app.get('/api/courses/requests', (req, res) => {
  const { student, faculty } = req.query;
  let enrollments = db.enrollments;
  if (student) enrollments = enrollments.filter(e => e.student_username === student);
  if (faculty) enrollments = enrollments.filter(e => e.faculty_username === faculty);
  res.json({ success: true, enrollments });
});

app.post('/api/courses/approve', (req, res) => {
  const { enrollment_id, status } = req.body;
  const enrollment = db.enrollments.find(e => e.id === enrollment_id);
  if (!enrollment) return res.json({ success: false, message: 'Enrollment not found.' });
  enrollment.status = status;
  res.json({ success: true, message: `Enrollment ${status} successfully.` });
});

// ==========================================================================
// SUBMISSION ROUTES
// ==========================================================================
app.get('/api/submissions/list', (req, res) => {
  const { student, faculty } = req.query;
  let subs = db.submissions;
  if (student) subs = subs.filter(s => s.student_username === student);
  if (faculty) {
    const facultyCourseIds = db.courses.filter(c => c.faculty_username === faculty).map(c => c.id);
    subs = subs.filter(s => facultyCourseIds.includes(s.course_id));
  }
  // Strip chat_history from list view to keep payload small
  const lightweight = subs.map(({ chat_history, ...rest }) => rest);
  res.json({ success: true, submissions: lightweight });
});

app.get('/api/submissions/detail', (req, res) => {
  const { id } = req.query;
  const sub = db.submissions.find(s => s.id === parseInt(id));
  if (!sub) return res.json({ success: false, message: 'Submission not found.' });
  res.json({ success: true, submission: sub });
});

app.post('/api/submissions/create', (req, res) => {
  const { course_id, student_username, student_name, title, description, pdf_filename, pdf_content, special_code } = req.body;
  const course = db.courses.find(c => c.id === parseInt(course_id));
  if (!course) return res.json({ success: false, message: 'Invalid course.' });

  const sub = {
    id: nextSubmissionId++,
    course_id: parseInt(course_id),
    student_username,
    student_name,
    title,
    description,
    pdf_filename: pdf_filename || `${title.replace(/\s+/g, '_')}_Report.pdf`,
    pdf_content,
    special_code,
    submitted_at: new Date().toISOString().split('T')[0],
    status: 'pending',
    has_ai_eval: false,
    ai_eval: null,
    has_faculty_eval: false,
    faculty_eval: null,
    has_final_eval: false,
    final_eval: null,
    chat_history: [
      { sender: 'AI', message: `Hello! I have received the new submission "${title}" by ${student_name}. I am ready to evaluate it once you trigger the AI evaluation. After evaluation, feel free to ask me questions about any rubric area or my scoring rationale.` },
    ],
  };
  db.submissions.push(sub);
  res.json({ success: true, message: 'Project submitted successfully!' });
});

// ==========================================================================
// AI EVALUATION ROUTE
// ==========================================================================
app.post('/api/evaluate/ai', (req, res) => {
  const { submission_id } = req.body;
  const sub = db.submissions.find(s => s.id === submission_id);
  if (!sub) return res.json({ success: false, message: 'Submission not found.' });

  // Analyze PDF content to generate contextual scores
  const content = (sub.pdf_content || '').toLowerCase();
  const desc = (sub.description || '').toLowerCase();
  const combined = content + ' ' + desc;

  // Rubric 1: Existing Project Check & Novelty (15-30)
  let r1 = 22;
  if (combined.includes('novel') || combined.includes('first') || combined.includes('surpass') || combined.includes('outperform')) r1 += 3;
  if (combined.includes('existing') || combined.includes('baseline') || combined.includes('comparison')) r1 += 2;
  if (combined.includes('literature review') || combined.includes('chapter 2')) r1 += 2;
  if (combined.includes('contribution') || combined.includes('our approach')) r1 += 1;
  r1 = Math.min(30, Math.max(15, r1));

  // Rubric 2: Code Quality & Techniques (15-30)
  let r2 = 20;
  if (combined.includes('code') || combined.includes('implementation') || combined.includes('snippet')) r2 += 3;
  if (combined.includes('class ') || combined.includes('def ') || combined.includes('function') || combined.includes('import ')) r2 += 2;
  if (combined.includes('clean') || combined.includes('efficient') || combined.includes('modular') || combined.includes('well-organized')) r2 += 2;
  if (combined.includes('python') || combined.includes('pytorch') || combined.includes('tensorflow') || combined.includes('keras')) r2 += 2;
  if (combined.includes('dropout') || combined.includes('batch') || combined.includes('optimizer')) r2 += 1;
  r2 = Math.min(30, Math.max(15, r2));

  // Rubric 3: Modules & Output (0-20)
  let r3 = 10;
  if (combined.includes('module') || combined.includes('chapter 5') || combined.includes('results')) r3 += 3;
  if (combined.includes('output') || combined.includes('auc') || combined.includes('f1') || combined.includes('accuracy')) r3 += 2;
  if (combined.includes('confusion matrix') || combined.includes('roc') || combined.includes('chart') || combined.includes('graph')) r3 += 2;
  if (combined.includes('dashboard') || combined.includes('visualization') || combined.includes('api')) r3 += 2;
  r3 = Math.min(20, Math.max(0, r3));

  // Rubric 4: Documentation & Scope (0-20)
  let r4 = 10;
  if (combined.includes('abstract') || combined.includes('introduction') || combined.includes('conclusion')) r4 += 3;
  if (combined.includes('chapter') || combined.includes('section')) r4 += 2;
  if (combined.includes('future work') || combined.includes('limitation')) r4 += 1;
  if (combined.includes('literature') || combined.includes('reference') || combined.includes('citation')) r4 += 2;
  if (combined.includes('scope') || combined.includes('methodology')) r4 += 2;
  r4 = Math.min(20, Math.max(0, r4));

  const total = r1 + r2 + r3 + r4;
  const is_pass = total >= 50;

  const evaluation = {
    r1, r1_fb: generateFeedback('r1', r1, content),
    r2, r2_fb: generateFeedback('r2', r2, content),
    r3, r3_fb: generateFeedback('r3', r3, content),
    r4, r4_fb: generateFeedback('r4', r4, content),
    total, is_pass,
    notes: `AI evaluation completed. Total score: ${total}/100. Status: ${is_pass ? 'PASS' : 'FAIL'}. Analysis based on full PDF content inspection.`,
  };

  sub.has_ai_eval = true;
  sub.ai_eval = evaluation;
  sub.status = 'evaluated';

  // Add AI evaluation summary to chat
  sub.chat_history.push({
    sender: 'AI',
    message: `I have completed the AI evaluation of "${sub.title}". Here is a summary:\n\n` +
      `📊 Rubric 1 (Novelty): ${r1}/30\n📊 Rubric 2 (Code Quality): ${r2}/30\n` +
      `📊 Rubric 3 (Modules & Output): ${r3}/20\n📊 Rubric 4 (Documentation): ${r4}/20\n\n` +
      `Total: ${total}/100 — ${is_pass ? '✅ PASS' : '❌ FAIL'}\n\n` +
      `Feel free to ask me about any specific rubric, why I scored a particular way, or suggest score adjustments.`,
  });

  res.json({ success: true, evaluation });
});

function generateFeedback(rubric, score, content) {
  const feedbackMap = {
    r1: {
      high: 'Strong novelty demonstrated. The project shows clear differentiation from existing systems with meaningful improvements over cited baselines. The literature review effectively identifies gaps that this work addresses.',
      mid: 'Moderate novelty. Some differentiation from existing work, but the improvements over baselines could be more clearly articulated. Consider strengthening the comparison with prior art.',
      low: 'Limited novelty detected. The project closely resembles existing systems without significant differentiation. More analysis of how this work advances beyond current solutions is needed.',
    },
    r2: {
      high: 'Clean, well-structured code with proper use of modern frameworks and techniques. Implementation follows best practices with clear separation of concerns and proper error handling.',
      mid: 'Satisfactory code quality. The implementation is functional but could benefit from better organization, more comments, or improved error handling in some areas.',
      low: 'Code quality needs improvement. Issues with organization, missing error handling, or unclear variable naming. Consider refactoring for better maintainability.',
    },
    r3: {
      high: 'Excellent modular decomposition with clear system components. Outputs are comprehensive with proper evaluation metrics, visualizations, and practical demonstrations.',
      mid: 'Adequate modules and outputs. The system has identifiable components, but outputs could be more comprehensive or better documented.',
      low: 'Modules and outputs need significant improvement. System components are unclear or outputs lack proper evaluation metrics and demonstrations.',
    },
    r4: {
      high: 'Well-documented report with clear structure, comprehensive literature review, and appropriate project scope. All chapters flow logically and support the project\'s objectives.',
      mid: 'Acceptable documentation. The report covers the essentials but could benefit from more detail in certain sections or a broader scope analysis.',
      low: 'Documentation is inadequate. Missing key sections, insufficient literature review, or unclear project scope. Significant improvements needed in report structure.',
    },
  };

  const tier = score >= 22 || (score >= 14 && rubric === 'r3') || (score >= 14 && rubric === 'r4') ? 'high'
    : score >= 17 || (score >= 8 && rubric === 'r3') || (score >= 8 && rubric === 'r4') ? 'mid'
    : 'low';

  return feedbackMap[rubric][tier];
}

// ==========================================================================
// FACULTY EVALUATION ROUTE
// ==========================================================================
app.post('/api/evaluate/faculty', (req, res) => {
  const { submission_id, is_final, rubric1_score, rubric1_feedback, rubric2_score, rubric2_feedback, rubric3_score, rubric3_feedback, rubric4_score, rubric4_feedback, evaluator_notes } = req.body;
  const sub = db.submissions.find(s => s.id === submission_id);
  if (!sub) return res.json({ success: false, message: 'Submission not found.' });

  const evalData = {
    r1: rubric1_score, r1_fb: rubric1_feedback,
    r2: rubric2_score, r2_fb: rubric2_feedback,
    r3: rubric3_score, r3_fb: rubric3_feedback,
    r4: rubric4_score, r4_fb: rubric4_feedback,
    total: rubric1_score + rubric2_score + rubric3_score + rubric4_score,
    is_pass: (rubric1_score + rubric2_score + rubric3_score + rubric4_score) >= 50,
    notes: evaluator_notes,
  };

  if (is_final === 'true' || is_final === true) {
    sub.has_final_eval = true;
    sub.final_eval = evalData;
    sub.status = 'graded';
    sub.chat_history.push({
      sender: 'AI',
      message: `🔒 The final grade has been published: ${evalData.total}/100 (${evalData.is_pass ? 'PASS' : 'FAIL'}). This grade is now locked and visible to the student in their portal. Thank you for the collaboration!`,
    });
  } else {
    sub.has_faculty_eval = true;
    sub.faculty_eval = evalData;
    sub.chat_history.push({
      sender: 'AI',
      message: `I see you have saved your faculty evaluation with a total of ${evalData.total}/100. ${evalData.is_pass ? 'The student passes.' : 'The student does not pass the threshold.'} You can continue our discussion or finalize the grade when ready.`,
    });
  }

  res.json({ success: true, message: is_final === 'true' ? 'Grade finalized and published to student!' : 'Faculty evaluation saved.' });
});

// ==========================================================================
// ANALYTICS ROUTE
// ==========================================================================
app.get('/api/analytics', (req, res) => {
  const { faculty } = req.query;
  const facultyCourses = db.courses.filter(c => c.faculty_username === faculty);
  const courseIds = facultyCourses.map(c => c.id);

  const enrollments = db.enrollments.filter(e => courseIds.includes(e.course_id));
  const submissions = db.submissions.filter(s => courseIds.includes(s.course_id));

  const totalEnrolled = enrollments.filter(e => e.status === 'accepted').length;
  const totalSubmitted = submissions.length;
  const pendingGrading = submissions.filter(s => !s.has_final_eval).length;

  const gradedSubs = submissions.filter(s => s.has_final_eval || s.has_faculty_eval || s.has_ai_eval);
  const scores = gradedSubs.map(s => {
    const ev = s.has_final_eval ? s.final_eval : (s.has_faculty_eval ? s.faculty_eval : s.ai_eval);
    return ev ? ev.total : 0;
  });

  const classAverage = scores.length > 0 ? scores.reduce((a, b) => a + b, 0) / scores.length : 0;
  const submissionRatio = totalEnrolled > 0 ? (totalSubmitted / totalEnrolled) * 100 : 0;

  const gradeA = scores.filter(s => s >= 85).length;
  const gradeB = scores.filter(s => s >= 70 && s < 85).length;
  const gradeC = scores.filter(s => s >= 50 && s < 70).length;
  const gradeFail = scores.filter(s => s < 50).length;

  res.json({
    success: true,
    analytics: {
      total_enrolled: totalEnrolled,
      total_submitted: totalSubmitted,
      pending_grading: pendingGrading,
      submission_ratio: submissionRatio,
      class_average: classAverage,
      graded_count: scores.length,
      grade_a: gradeA,
      grade_b: gradeB,
      grade_c: gradeC,
      grade_fail: gradeFail,
    },
  });
});

// ==========================================================================
// CHAT ROUTES — Context-Aware AI Chatbot
// ==========================================================================
app.get('/api/chat/history', (req, res) => {
  const { submission_id } = req.query;
  const sub = db.submissions.find(s => s.id === parseInt(submission_id));
  if (!sub) return res.json({ success: false, message: 'Submission not found.' });
  res.json({ success: true, messages: sub.chat_history });
});

app.post('/api/chat/send', (req, res) => {
  const { submission_id, message } = req.body;
  const sub = db.submissions.find(s => s.id === submission_id);
  if (!sub) return res.json({ success: false, message: 'Submission not found.' });

  // Save faculty message
  sub.chat_history.push({ sender: 'FACULTY', message });

  // Generate context-aware AI reply
  const reply = generateSmartReply(sub, message);
  sub.chat_history.push({ sender: 'AI', message: reply.text });

  res.json({ success: true, reply: { text: reply.text, ...reply.scoreAdjustments } });
});

// ==========================================================================
// SMART CHATBOT — Responds Based on the User's Actual Question
// ==========================================================================
function generateSmartReply(submission, userMessage) {
  const msg = userMessage.toLowerCase().trim();
  const content = (submission.pdf_content || '').toLowerCase();
  const desc = (submission.description || '').toLowerCase();
  const evalData = submission.has_ai_eval ? submission.ai_eval : (submission.has_faculty_eval ? submission.faculty_eval : null);

  // ---- Topic Detection ----
  // NOTE: isAboutScore/Change must be checked FIRST before specific rubric topics
  // to avoid false positives (e.g. "rubric 2" matching "module" via substring).
  const isAboutScore = /\b(score|grade|mark|point|total|evaluat|rating|rubric)\b/.test(msg);
  const isSuggestingChange = /\b(increase|decrease|adjust|change|modify|raise|lower|suggest|revise|should\s*(be|have)|too\s*(high|low)|bump|add|remove)\b/.test(msg);
  const isAboutPass = /\b(pass|fail|threshold|minimum|cutoff|qualify)\b/.test(msg);
  const isAboutNovelty = /\b(novel|novelty|rubric\s*1|r1|existing\s*project|existing\s*system|prior\s*work|baseline|literature|comparison|original|unique|different)\b/.test(msg);
  const isAboutCode = /\b(code|coding|quality|technique|implementation|programming|algorithm|function|class|model|architecture|pytorch|tensorflow|python|clean|efficient|bug|error|refactor)\b/.test(msg);
  const isAboutModules = /\b(module[s]?|output[s]?|result[s]?|system\s*component|architecture|pipeline|dashboard|api|endpoint|feature[s]?|deployment|demonstration|metric[s]?|performance|structured|decomposition)\b/.test(msg);
  const isAboutDocumentation = /\b(document|report|documentation|chapter|section|literature\s*review|abstract|introduction|conclusion|scope|reference|citation|write|writing|structure|format)\b/.test(msg);
  const isAskingWhy = /\b(why|reason|rationale|justif|explain|tell\s*me|how\s*did|basis|because|what\s*made)\b/.test(msg);
  const isAskingHelp = /\b(help|what\s*can|how\s*do|guide|assist|support|capabilities|what\s*do\s*you)\b/.test(msg);
  const isGreeting = /\b(hi|hello|hey|good\s*(morning|afternoon|evening)|greetings|howdy|what'?s\s*up)\b/.test(msg);
  const isThanks = /\b(thank|thanks|appreciate|great|good\s*job|well\s*done|nice|excellent|perfect)\b/.test(msg);
  const isSummary = /\b(summary|summarize|overview|recap|brief|overall|complete\s*picture)\b/.test(msg);

  // ---- Greeting ----
  if (isGreeting && !isAboutScore && !isAboutNovelty && !isAboutCode) {
    return { text: `Hello! I am your AI Co-Evaluator for "${submission.title}". I can discuss:\n\n• Rubric scoring (novelty, code quality, modules, documentation)\n• Why I assigned specific scores\n• Score adjustments you suggest\n• Overall evaluation summary\n\nWhat would you like to discuss?` };
  }

  // ---- Thanks ----
  if (isThanks && !isAboutScore) {
    return { text: `You're welcome! I'm here to help ensure a fair and thorough evaluation. Feel free to ask about any rubric area, suggest score changes, or discuss the project further.` };
  }

  // ---- Help ----
  if (isAskingHelp && !isAboutScore) {
    return { text: `I can help you with:\n\n1️⃣ **Discuss rubric scores** — ask about any specific rubric area\n2️⃣ **Explain my reasoning** — ask "why did you score X for rubric Y?"\n3️⃣ **Suggest adjustments** — tell me to increase/decrease a score and I'll respond with analysis\n4️⃣ **Summarize** — ask for an overall evaluation summary\n5️⃣ **Compare** — ask how this project compares to existing work\n\nJust ask your question naturally!` };
  }

  // ---- Score Adjustment Suggestions (checked BEFORE specific rubric topics) ----
  if (isSuggestingChange && isAboutScore) {
    if (!evalData) return { text: 'The project has not been evaluated yet. Trigger the AI evaluation first before discussing score adjustments.' };

    let target = null;
    let direction = null;
    let amount = null;

    if (/\b(rubric\s*1|r1|novelty|existing)\b/.test(msg)) target = 'r1';
    else if (/\b(rubric\s*2|r2|code|quality|technique)\b/.test(msg)) target = 'r2';
    else if (/\b(rubric\s*3|r3|module|output)\b/.test(msg)) target = 'r3';
    else if (/\b(rubric\s*4|r4|document|scope|report)\b/.test(msg)) target = 'r4';
    else if (/\b(total|overall)\b/.test(msg)) target = 'total';

    if (/\b(increase|raise|bump|add|higher|more|too\s*low|should\s*be\s*higher)\b/.test(msg)) direction = 'up';
    else if (/\b(decrease|reduce|lower|subtract|less|too\s*high|should\s*be\s*lower)\b/.test(msg)) direction = 'down';

    const numMatch = msg.match(/(\d+)/);
    if (numMatch) amount = parseInt(numMatch[1]);

    if (!target || !direction) {
      return { text: `I understand you'd like to discuss score adjustments. Could you be more specific?\n\nExamples:\n• \"Increase rubric 1 by 3 points\"\n• \"Decrease code quality score to 20\"\n• \"The total should be higher\"\n\nWhich rubric and direction would you like to adjust?` };
    }

    const currentScore = target === 'total' ? evalData.total : evalData[target];
    const maxScores = { r1: 30, r2: 30, r3: 20, r4: 20, total: 100 };
    const rubricNames = { r1: 'Rubric 1 (Novelty)', r2: 'Rubric 2 (Code Quality)', r3: 'Rubric 3 (Modules & Output)', r4: 'Rubric 4 (Documentation)', total: 'Total Score' };

    let newScore = currentScore;

    if (amount && target !== 'total') {
      newScore = direction === 'up' ? Math.min(maxScores[target], currentScore + amount) : Math.max(0, currentScore - amount);
    } else {
      newScore = direction === 'up' ? Math.min(maxScores[target], currentScore + 2) : Math.max(0, currentScore - 2);
    }

    if (target === 'total') {
      return { text: `📊 **Total Score Adjustment Discussion**\n\nCurrent total: ${evalData.total}/100\n\nThe total is the sum of all rubrics. To adjust the total, we need to modify individual rubrics:\n\n• Rubric 1 (Novelty): ${evalData.r1}/30\n• Rubric 2 (Code Quality): ${evalData.r2}/30\n• Rubric 3 (Modules & Output): ${evalData.r3}/20\n• Rubric 4 (Documentation): ${evalData.r4}/20\n\nWhich specific rubric would you like me to adjust?` };
    }

    const changeDir = direction === 'up' ? 'increased' : 'decreased';
    let response = `📊 **${rubricNames[target]} Adjustment**\n\nCurrent: ${currentScore}/${maxScores[target]}\nProposed: ${newScore}/${maxScores[target]}\n\nI've ${changeDir} the score.\n\n`;

    const reasons = {
      r1: newScore > currentScore ? 'The increase reflects stronger evidence of novelty. The project shows better differentiation from existing systems.' : 'The decrease accounts for the novelty claim needing stronger support with more rigorous comparison.',
      r2: newScore > currentScore ? 'Upon reconsideration, the code quality is stronger. The implementation shows good practices and structure.' : 'On reflection, the code quality has areas needing improvement.',
      r3: newScore > currentScore ? 'The module decomposition and outputs are more comprehensive than initially assessed.' : 'The modules and outputs could be stronger with better definition.',
      r4: newScore > currentScore ? 'The documentation is more thorough than initially scored. The report structure is clear.' : 'The documentation could be more comprehensive with more detail.',
    };
    response += reasons[target] || '';

    const adjustedEval = { ...evalData, [target]: newScore };
    const newTotal = adjustedEval.r1 + adjustedEval.r2 + adjustedEval.r3 + adjustedEval.r4;
    const newPass = newTotal >= 50;
    response += `\n\n📈 **Updated Total: ${newTotal}/100 (${newPass ? 'PASS ✅' : 'FAIL ❌'})**`;
    if (newPass !== evalData.is_pass) {
      response += newPass ? '\n\n🎉 This change moves the student from FAIL to PASS!' : '\n\n⚠️ This change moves the student from PASS to FAIL!';
    }

    return { text: response, scoreAdjustments: { [`suggested_${target}`]: newScore } };
  }

  // ---- Summary (checked after score adjustment) ----
  if (isSummary) {
    const total = evalData ? evalData.total : 'N/A';
    const pass = evalData ? (evalData.is_pass ? 'PASS' : 'FAIL') : 'PENDING';
    return { text: `📋 **Evaluation Summary for "${submission.title}"**\n\nStudent: ${submission.student_name}\nStatus: ${pass} (${total}/100)\n\n` +
      (evalData ? `• Rubric 1 (Novelty): ${evalData.r1}/30\n• Rubric 2 (Code Quality): ${evalData.r2}/30\n• Rubric 3 (Modules & Output): ${evalData.r3}/20\n• Rubric 4 (Documentation): ${evalData.r4}/20\n\n` +
        `**Key Observations:**\n${evalData.r1 >= 25 ? '✅ Strong novelty and differentiation from existing work.' : evalData.r1 >= 20 ? '⚠️ Moderate novelty — could strengthen comparison with prior art.' : '❌ Limited novelty — needs clearer differentiation.'}\n` +
        `${evalData.r2 >= 25 ? '✅ Clean, well-structured implementation.' : evalData.r2 >= 20 ? '⚠️ Satisfactory code quality with room for improvement.' : '❌ Code quality needs attention.'}\n` +
        `${evalData.r3 >= 15 ? '✅ Good modular design and outputs.' : evalData.r3 >= 10 ? '⚠️ Adequate modules but outputs could be stronger.' : '❌ Module decomposition and outputs need work.'}\n` +
        `${evalData.r4 >= 15 ? '✅ Well-documented with appropriate scope.' : evalData.r4 >= 10 ? '⚠️ Documentation acceptable but could be more thorough.' : '❌ Documentation is insufficient.'}`
      : 'I have not yet evaluated this project. Trigger the AI evaluation first, then we can discuss the results.') };
  }

  // ---- About Pass/Fail ----
  if (isAboutPass && !isAboutNovelty && !isAboutCode) {
    if (!evalData) return { text: 'The project has not been evaluated yet. Please trigger the AI evaluation first, then we can discuss the pass/fail status.' };
    return { text: `📊 **Pass/Fail Analysis**\n\nCurrent Total: ${evalData.total}/100\nThreshold: 50/100\nStatus: ${evalData.is_pass ? '✅ PASS' : '❌ FAIL'}\n\n` +
      (evalData.is_pass ? `The project meets the minimum threshold with a comfortable margin of ${evalData.total - 50} points. ` +
        (evalData.total >= 70 ? 'The score is solid across all rubrics.' : 'While passing, some rubric areas could be strengthened.') :
        `The project falls ${50 - evalData.total} points below the passing threshold. Key areas needing improvement:\n` +
        `${evalData.r1 < 20 ? '• Rubric 1 (Novelty): Needs stronger differentiation from existing work\n' : ''}` +
        `${evalData.r2 < 20 ? '• Rubric 2 (Code Quality): Implementation needs improvement\n' : ''}` +
        `${evalData.r3 < 12 ? '• Rubric 3 (Modules): System modules and outputs need work\n' : ''}` +
        `${evalData.r4 < 12 ? '• Rubric 4 (Documentation): Report and documentation need enhancement\n' : ''}`) +
      `\nWould you like to discuss specific rubric adjustments?` };
  }

  // ---- Novelty Discussion ----
  if (isAboutNovelty) {
    const noveltyScore = evalData ? evalData.r1 : null;
    const hasLitReview = content.includes('literature review') || content.includes('chapter 2') || content.includes('existing');
    const hasBaseline = content.includes('baseline') || content.includes('comparison') || content.includes('outperform') || content.includes('surpass');
    const hasContribution = content.includes('contribution') || content.includes('our approach') || content.includes('our method');

    let analysis = `🔍 **Rubric 1: Existing Project Check & Novelty**\n\n`;
    if (noveltyScore !== null) {
      analysis += `Current Score: ${noveltyScore}/30\n\n`;
    }

    if (hasLitReview) {
      analysis += `✅ The project includes a literature review section that identifies existing systems.\n`;
    } else {
      analysis += `⚠️ The literature review section is not clearly defined.\n`;
    }

    if (hasBaseline) {
      analysis += `✅ The project compares against baseline methods and claims improvements.\n`;
    } else {
      analysis += `⚠️ Baseline comparisons could be more explicit.\n`;
    }

    if (hasContribution) {
      analysis += `✅ The project clearly states its contribution/novel approach.\n`;
    } else {
      analysis += `⚠️ The unique contribution could be more clearly articulated.\n`;
    }

    // Check for specific existing systems mentioned
    const existingSystems = content.match(/\b(rajkomar|harutyunyan|golas|mimic|featforge|logg|etl|xgboost|logistic\s*regression)\b/gi);
    if (existingSystems) {
      analysis += `\n📚 Referenced existing systems: ${[...new Set(existingSystems)].join(', ')}\n`;
    }

    if (isAskingWhy && noveltyScore !== null) {
      analysis += `\n**Why I scored ${noveltyScore}/30:**\n`;
      if (noveltyScore >= 25) {
        analysis += `The project demonstrates strong novelty. It clearly identifies gaps in existing literature and presents a differentiated approach. The comparison with multiple baselines strengthens the novelty claim.`;
      } else if (noveltyScore >= 20) {
        analysis += `The novelty is moderate. While some differentiation exists, the comparison with prior work could be more rigorous. Consider whether the claimed improvements are statistically significant.`;
      } else {
        analysis += `The novelty is limited. The project appears to closely follow existing approaches without clear differentiation. A stronger analysis of what makes this work unique is needed.`;
      }
    }

    return { text: analysis };
  }

  // ---- Code Quality Discussion ----
  if (isAboutCode) {
    const codeScore = evalData ? evalData.r2 : null;
    const hasCode = content.includes('```') || content.includes('code') || content.includes('implementation') || content.includes('snippet');
    const hasClass = content.includes('class ') || content.includes('def ') || content.includes('function');
    const hasFramework = content.includes('pytorch') || content.includes('tensorflow') || content.includes('keras') || content.includes('sklearn');
    const hasGoodPractice = content.includes('dropout') || content.includes('batch') || content.includes('optimizer') || content.includes('normalization') || content.includes('regularization');

    let analysis = `💻 **Rubric 2: Code Quality & Techniques**\n\n`;
    if (codeScore !== null) {
      analysis += `Current Score: ${codeScore}/30\n\n`;
    }

    if (hasCode) {
      analysis += `✅ The report includes code snippets/implementation details.\n`;
    } else {
      analysis += `⚠️ No code snippets found in the report. Including implementation code strengthens this rubric.\n`;
    }

    if (hasClass) {
      analysis += `✅ Code shows proper class/function structure.\n`;
    }

    if (hasFramework) {
      analysis += `✅ Uses established ML frameworks (${content.includes('pytorch') ? 'PyTorch' : content.includes('tensorflow') ? 'TensorFlow' : 'ML framework'}).`;
      if (hasGoodPractice) analysis += ` Includes proper techniques (dropout, batch normalization, etc.).`;
      analysis += `\n`;
    } else if (hasCode) {
      analysis += `⚠️ Could benefit from using established ML frameworks for better reproducibility.\n`;
    }

    if (isAskingWhy && codeScore !== null) {
      analysis += `\n**Why I scored ${codeScore}/30:**\n`;
      if (codeScore >= 25) {
        analysis += `The code is clean, well-structured, and uses modern techniques. Proper modular design with clear separation of concerns.`;
      } else if (codeScore >= 20) {
        analysis += `Satisfactory code quality. The implementation works but could be cleaner in some areas. Consider adding more comments and improving error handling.`;
      } else {
        analysis += `Code quality needs improvement. Issues with organization, unclear variable naming, or missing best practices.`;
      }
    }

    return { text: analysis };
  }

  // ---- Modules & Output Discussion ----
  if (isAboutModules) {
    const moduleScore = evalData ? evalData.r3 : null;
    const hasModules = content.includes('module') || content.includes('component') || content.includes('chapter 5') || content.includes('system');
    const hasMetrics = content.includes('auc') || content.includes('f1') || content.includes('accuracy') || content.includes('precision') || content.includes('recall');
    const hasViz = content.includes('dashboard') || content.includes('visualization') || content.includes('chart') || content.includes('graph') || content.includes('plot');
    const hasAPI = content.includes('api') || content.includes('endpoint') || content.includes('deployment') || content.includes('fastapi') || content.includes('flask');

    let analysis = `📦 **Rubric 3: Modules & Output**\n\n`;
    if (moduleScore !== null) {
      analysis += `Current Score: ${moduleScore}/20\n\n`;
    }

    if (hasModules) {
      const moduleMatches = content.match(/module\s*\d+[^.\n]*/gi);
      if (moduleMatches) {
        analysis += `📋 Identified modules:\n${moduleMatches.map(m => `• ${m.trim()}`).join('\n')}\n\n`;
      } else {
        analysis += `✅ The project describes system modules/components.\n`;
      }
    } else {
      analysis += `⚠️ Clear module decomposition is not evident in the report.\n`;
    }

    if (hasMetrics) {
      const metricMatches = content.match(/(auc[-\s]*roc|auc|f1[-\s]*score|accuracy|precision|recall|loss)[:\s]+[\d.]+/gi);
      if (metricMatches) {
        analysis += `📊 Metrics found: ${metricMatches.slice(0, 4).join('; ')}\n`;
      }
    } else {
      analysis += `⚠️ No quantitative evaluation metrics found.\n`;
    }

    if (hasViz) analysis += `✅ Includes visualization/dashboard components.\n`;
    if (hasAPI) analysis += `✅ Includes API/deployment component.\n`;

    if (isAskingWhy && moduleScore !== null) {
      analysis += `\n**Why I scored ${moduleScore}/20:**\n`;
      if (moduleScore >= 15) {
        analysis += `Good modular decomposition with clear system components and comprehensive outputs including metrics and visualizations.`;
      } else if (moduleScore >= 10) {
        analysis += `Adequate modules but outputs could be more comprehensive. Consider adding more evaluation metrics or visualizations.`;
      } else {
        analysis += `Module structure and outputs need significant improvement. The system components are unclear.`;
      }
    }

    return { text: analysis };
  }

  // ---- Documentation Discussion ----
  if (isAboutDocumentation) {
    const docScore = evalData ? evalData.r4 : null;
    const hasChapters = content.includes('chapter') || content.includes('section');
    const hasAbstract = content.includes('abstract');
    const hasConclusion = content.includes('conclusion');
    const hasFutureWork = content.includes('future work') || content.includes('future direction');
    const hasReferences = content.includes('reference') || content.includes('citation') || content.includes('bibliography');

    let analysis = `📝 **Rubric 4: Documentation & Scope**\n\n`;
    if (docScore !== null) {
      analysis += `Current Score: ${docScore}/20\n\n`;
    }

    if (hasAbstract) analysis += `✅ Has an abstract section.\n`;
    if (hasChapters) analysis += `✅ Well-structured with chapters/sections.\n`;
    if (hasConclusion) analysis += `✅ Includes conclusion.\n`;
    if (hasFutureWork) analysis += `✅ Discusses future work/limitations.\n`;
    if (hasReferences) analysis += `✅ Includes references/citations.\n`;

    const missing = [];
    if (!hasAbstract) missing.push('abstract');
    if (!hasChapters) missing.push('clear chapter structure');
    if (!hasConclusion) missing.push('conclusion');
    if (!hasReferences) missing.push('references');
    if (missing.length > 0) {
      analysis += `\n⚠️ Missing: ${missing.join(', ')}\n`;
    }

    if (isAskingWhy && docScore !== null) {
      analysis += `\n**Why I scored ${docScore}/20:**\n`;
      if (docScore >= 15) {
        analysis += `Well-documented report with logical structure, comprehensive literature review, and appropriate scope.`;
      } else if (docScore >= 10) {
        analysis += `Acceptable documentation covering the essentials, but some sections need more detail or the scope could be broader.`;
      } else {
        analysis += `Documentation needs significant improvement. Missing key sections or insufficient detail.`;
      }
    }

    return { text: analysis };
  }

  // ---- Summary (checked after score adjustment) ----
  if (isSummary) {
    if (!evalData) return { text: 'The project has not been evaluated yet. Trigger the AI evaluation first before discussing score adjustments.' };

    // Detect which rubric and direction
    let target = null;
    let direction = null;
    let amount = null;

    if (/\b(rubric\s*1|r1|novelty|existing)\b/.test(msg)) target = 'r1';
    else if (/\b(rubric\s*2|r2|code|quality|technique)\b/.test(msg)) target = 'r2';
    else if (/\b(rubric\s*3|r3|module|output)\b/.test(msg)) target = 'r3';
    else if (/\b(rubric\s*4|r4|document|scope|report)\b/.test(msg)) target = 'r4';
    else if (/\b(total|overall|overall\s*score)\b/.test(msg)) target = 'total';

    if (/\b(increase|raise|bump|add|higher|more|too\s*low|should\s*be\s*higher)\b/.test(msg)) direction = 'up';
    else if (/\b(decrease|reduce|lower|subtract|less|too\s*high|should\s*be\s*lower)\b/.test(msg)) direction = 'down';

    // Try to extract a number
    const numMatch = msg.match(/(\d+)/);
    if (numMatch) amount = parseInt(numMatch[1]);

    if (!target || !direction) {
      return { text: `I understand you'd like to discuss score adjustments. Could you be more specific?\n\nExamples:\n• "Increase rubric 1 by 3 points"\n• "Decrease code quality score to 20"\n• "The total should be higher"\n\nWhich rubric and direction would you like to adjust?` };
    }

    const currentScore = target === 'total' ? evalData.total : evalData[target];
    const maxScores = { r1: 30, r2: 30, r3: 20, r4: 20, total: 100 };
    const rubricNames = { r1: 'Rubric 1 (Novelty)', r2: 'Rubric 2 (Code Quality)', r3: 'Rubric 3 (Modules & Output)', r4: 'Rubric 4 (Documentation)', total: 'Total Score' };

    let newScore = currentScore;
    let response = '';

    if (amount && target !== 'total') {
      if (direction === 'up') {
        newScore = Math.min(maxScores[target], currentScore + amount);
      } else {
        newScore = Math.max(0, currentScore - amount);
      }
    } else {
      // Default adjustment
      if (direction === 'up') {
        newScore = Math.min(maxScores[target], currentScore + 2);
      } else {
        newScore = Math.max(0, currentScore - 2);
      }
    }

    if (target === 'total') {
      // For total, explain the aggregate
      response = `📊 **Total Score Adjustment Discussion**\n\nCurrent total: ${evalData.total}/100\n\nThe total is the sum of all rubrics. To adjust the total, we need to modify individual rubrics:\n\n• Rubric 1 (Novelty): ${evalData.r1}/30\n• Rubric 2 (Code Quality): ${evalData.r2}/30\n• Rubric 3 (Modules & Output): ${evalData.r3}/20\n• Rubric 4 (Documentation): ${evalData.r4}/20\n\nWhich specific rubric would you like me to adjust? I can provide analysis for any area.`;
    } else {
      const changeDir = direction === 'up' ? 'increased' : 'decreased';
      response = `📊 **${rubricNames[target]} Adjustment**\n\nCurrent: ${currentScore}/${maxScores[target]}\nProposed: ${newScore}/${maxScores[target]}\n\nI've ${changeDir} the score. Here's my analysis:\n\n`;

      if (target === 'r1') {
        response += newScore > currentScore ?
          `The increase reflects stronger evidence of novelty in the project. The project shows better differentiation from existing systems than initially assessed.` :
          `The decrease accounts for the fact that the novelty claim could be better supported with more rigorous comparison to existing work.`;
      } else if (target === 'r2') {
        response += newScore > currentScore ?
          `Upon reconsideration, the code quality is stronger than initially scored. The implementation shows good practices and proper structure.` :
          `On reflection, the code quality has some areas that need improvement, justifying a lower score.`;
      } else if (target === 'r3') {
        response += newScore > currentScore ?
          `The module decomposition and outputs are more comprehensive than initially assessed. The system has clear, well-defined components.` :
          `The modules and outputs could be stronger. Some components need better definition or the outputs need more comprehensive evaluation.`;
      } else if (target === 'r4') {
        response += newScore > currentScore ?
          `The documentation is more thorough than initially scored. The report structure is clear and covers the necessary areas.` :
          `The documentation could be more comprehensive. Some sections need more detail or better organization.`;
      }

      // Calculate new total
      const adjustedEval = { ...evalData, [target]: newScore };
      const newTotal = adjustedEval.r1 + adjustedEval.r2 + adjustedEval.r3 + adjustedEval.r4;
      const newPass = newTotal >= 50;

      response += `\n\n📈 **Updated Total: ${newTotal}/100 (${newPass ? 'PASS ✅' : 'FAIL ❌'})**`;
      if (newPass !== evalData.is_pass) {
        response += newPass ? `\n\n🎉 This change moves the student from FAIL to PASS!` : `\n\n⚠️ This change moves the student from PASS to FAIL!`;
      }

      return { text: response, scoreAdjustments: { [`suggested_${target}`]: newScore } };
    }

    return { text: response };
  }

  // ---- Why questions (general) ----
  if (isAskingWhy && evalData) {
    return { text: `I'd be happy to explain my reasoning. Could you specify which rubric area you'd like me to elaborate on?\n\n• **Novelty (R1):** Why the originality score?\n• **Code Quality (R2):** Why the implementation score?\n• **Modules (R3):** Why the system design score?\n• **Documentation (R4):** Why the report quality score?\n• **Overall:** Why the total/pass-fail decision?\n\nJust ask about the specific area!` };
  }

  // ---- General Score Questions ----
  if (isAboutScore && !isSuggestingChange) {
    if (!evalData) return { text: 'The project has not been evaluated yet. Please trigger the AI evaluation first to generate scores.' };

    return { text: `📊 **Current Evaluation Scores**\n\n` +
      `• Rubric 1 (Existing Project & Novelty): ${evalData.r1}/30\n` +
      `• Rubric 2 (Code Quality & Techniques): ${evalData.r2}/30\n` +
      `• Rubric 3 (Modules & Output): ${evalData.r3}/20\n` +
      `• Rubric 4 (Documentation & Scope): ${evalData.r4}/20\n` +
      `• **Total: ${evalData.total}/100** — ${evalData.is_pass ? '✅ PASS' : '❌ FAIL'}\n\n` +
      `Ask me about any specific rubric, request score adjustments, or say "why" to understand my reasoning!` };
  }

  // ---- Fallback: Contextual generic response ----
  return { text: `I'm here to discuss the evaluation of "${submission.title}" by ${submission.student_name}.\n\nI can help with:\n• **Rubric analysis** — ask about novelty, code quality, modules, or documentation\n• **Score explanations** — ask "why did you score X for [rubric]?"\n• **Adjustments** — suggest changes like "increase rubric 1 by 3"\n• **Summary** — ask for an overall evaluation summary\n\nWhat would you like to discuss?` };
}

// ==========================================================================
// EXCEL EXPORT
// ==========================================================================
app.get('/api/export/excel', (req, res) => {
  const { faculty } = req.query;
  const facultyCourses = db.courses.filter(c => c.faculty_username === faculty);
  const courseIds = facultyCourses.map(c => c.id);
  const submissions = db.submissions.filter(s => courseIds.includes(s.course_id));

  let csv = 'ID,Student,Username,Project Title,Subject,Status,R1,R2,R3,R4,Total,Pass/Fail,Evaluator Notes\n';
  submissions.forEach(sub => {
    const evalObj = sub.has_final_eval ? sub.final_eval : (sub.has_faculty_eval ? sub.faculty_eval : sub.ai_eval);
    const course = db.courses.find(c => c.id === sub.course_id);
    csv += `${sub.id},"${sub.student_name}","${sub.student_username}","${sub.title}","${course ? course.subject_name : ''}",${sub.status},`;
    if (evalObj) {
      csv += `${evalObj.r1},${evalObj.r2},${evalObj.r3},${evalObj.r4},${evalObj.total},${evalObj.is_pass ? 'PASS' : 'FAIL'},"${(evalObj.notes || '').replace(/"/g, '""')}"\n`;
    } else {
      csv += `,,,,,PENDING,"Not yet evaluated"\n`;
    }
  });

  res.setHeader('Content-Type', 'text/csv');
  res.setHeader('Content-Disposition', 'attachment; filename="evalai_grades_export.csv"');
  res.send(csv);
});

// ==========================================================================
// START SERVER
// ==========================================================================
app.listen(PORT, '0.0.0.0', () => {
  console.log(`EvalAI Backend running on http://0.0.0.0:${PORT}`);
});
