// ==========================================================================
// Project Evaluation System - Frontend Application Logic (app.js)
// ==========================================================================

document.addEventListener('DOMContentLoaded', () => {
    let currentUser = null;
    let activeSubmissionId = null;

    // DOM Elements
    const authContainer = document.getElementById('authContainer');
    const appDashboard = document.getElementById('appDashboard');
    const studentDashboard = document.getElementById('studentDashboard');
    const facultyDashboard = document.getElementById('facultyDashboard');
    const userBar = document.getElementById('userBar');
    const roleBadge = document.getElementById('roleBadge');
    const userNameDisplay = document.getElementById('userNameDisplay');
    const logoutBtn = document.getElementById('logoutBtn');

    // Forms & Tabs
    const loginForm = document.getElementById('loginForm');
    const registerForm = document.getElementById('registerForm');
    const registerCard = document.getElementById('registerCard');
    const tabFaculty = document.getElementById('tabFaculty');
    const tabStudent = document.getElementById('tabStudent');
    const toggleRegisterLink = document.getElementById('toggleRegisterLink');
    const toggleLoginLink = document.getElementById('toggleLoginLink');
    const demoFacultyBtn = document.getElementById('demoFacultyBtn');
    const demoStudentBtn = document.getElementById('demoStudentBtn');

    // Modals
    const enrollModal = document.getElementById('enrollModal');
    const submitModal = document.getElementById('submitModal');
    const createCourseModal = document.getElementById('createCourseModal');
    const evalHubModal = document.getElementById('evalHubModal');
    const studentResultModal = document.getElementById('studentResultModal');

    // ----------------------------------------------------
    // Auth & Navigation Logic
    // ----------------------------------------------------
    tabFaculty.addEventListener('click', () => {
        tabFaculty.classList.add('active');
        tabStudent.classList.remove('active');
    });

    tabStudent.addEventListener('click', () => {
        tabStudent.classList.add('active');
        tabFaculty.classList.remove('active');
    });

    toggleRegisterLink.addEventListener('click', (e) => {
        e.preventDefault();
        loginForm.parentElement.style.display = 'none';
        registerCard.style.display = 'block';
    });

    toggleLoginLink.addEventListener('click', (e) => {
        e.preventDefault();
        registerCard.style.display = 'none';
        loginForm.parentElement.style.display = 'block';
    });

    demoFacultyBtn.addEventListener('click', () => {
        tabFaculty.click();
        document.getElementById('loginUsername').value = 'prof_smith';
        document.getElementById('loginPassword').value = 'pass123';
    });

    demoStudentBtn.addEventListener('click', () => {
        tabStudent.click();
        document.getElementById('loginUsername').value = 'alex_student';
        document.getElementById('loginPassword').value = 'pass123';
    });

    loginForm.addEventListener('submit', async (e) => {
        e.preventDefault();
        const username = document.getElementById('loginUsername').value;
        const password = document.getElementById('loginPassword').value;

        try {
            const res = await fetch('/api/login', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ username, password })
            });
            const data = await res.json();

            if (data.success) {
                currentUser = data.user;
                initUserSession();
            } else {
                alert(data.message || 'Login failed!');
            }
        } catch (err) {
            console.error('Login error:', err);
            alert('Could not connect to C++ backend server.');
        }
    });

    registerForm.addEventListener('submit', async (e) => {
        e.preventDefault();
        const full_name = document.getElementById('regFullName').value;
        const username = document.getElementById('regUsername').value;
        const password = document.getElementById('regPassword').value;
        const role = document.getElementById('regRole').value;

        try {
            const res = await fetch('/api/register', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ full_name, username, password, role })
            });
            const data = await res.json();
            if (data.success) {
                alert(data.message);
                toggleLoginLink.click();
            } else {
                alert(data.message);
            }
        } catch (err) {
            alert('Registration error.');
        }
    });

    logoutBtn.addEventListener('click', () => {
        currentUser = null;
        authContainer.style.display = 'flex';
        appDashboard.style.display = 'none';
        userBar.style.display = 'none';
    });

    function initUserSession() {
        authContainer.style.display = 'none';
        appDashboard.style.display = 'block';
        userBar.style.display = 'flex';

        roleBadge.textContent = currentUser.role === 'faculty' ? 'Faculty' : 'Student';
        userNameDisplay.textContent = currentUser.full_name;

        if (currentUser.role === 'student') {
            studentDashboard.style.display = 'block';
            facultyDashboard.style.display = 'none';
            loadStudentDashboard();
        } else {
            facultyDashboard.style.display = 'block';
            studentDashboard.style.display = 'none';
            loadFacultyDashboard();
        }
    }

    // Modal Close Buttons
    document.querySelectorAll('.close-btn, .closeModalBtn').forEach(btn => {
        btn.addEventListener('click', () => {
            document.querySelectorAll('.modal').forEach(m => m.classList.remove('active'));
        });
    });

    // ----------------------------------------------------
    // STUDENT PORTAL LOGIC
    // ----------------------------------------------------
    async function loadStudentDashboard() {
        // Fetch Enrollments
        try {
            const res = await fetch(`/api/courses/requests?student=${currentUser.username}`);
            const data = await res.json();
            const tableBody = document.getElementById('studentEnrollmentTable');
            tableBody.innerHTML = '';

            let acceptedCount = 0;
            if (data.success && data.enrollments) {
                data.enrollments.forEach(en => {
                    if (en.status === 'accepted') acceptedCount++;
                    const badgeClass = en.status === 'accepted' ? 'badge-success' : (en.status === 'pending' ? 'badge-warning' : 'badge-danger');
                    const row = document.createElement('tr');
                    row.innerHTML = `
                        <td><strong>${escapeHtml(en.subject_name)}</strong></td>
                        <td>${escapeHtml(en.faculty_name)}</td>
                        <td><code>${escapeHtml(en.course_code)}</code></td>
                        <td><span class="badge ${badgeClass}">${en.status.toUpperCase()}</span></td>
                        <td>${escapeHtml(en.requested_at)}</td>
                    `;
                    tableBody.appendChild(row);
                });
            }
            document.getElementById('stuCourseCount').textContent = acceptedCount;
        } catch (e) {
            console.error('Error loading student enrollments:', e);
        }

        // Fetch Submissions
        try {
            const res = await fetch(`/api/submissions/list?student=${currentUser.username}`);
            const data = await res.json();
            const grid = document.getElementById('studentSubmissionsList');
            grid.innerHTML = '';

            let subCount = 0;
            let gradedCount = 0;

            if (data.success && data.submissions) {
                subCount = data.submissions.length;
                data.submissions.forEach(sub => {
                    if (sub.status === 'graded') gradedCount++;

                    const evalObj = sub.has_final_eval ? sub.final_eval : (sub.has_faculty_eval ? sub.faculty_eval : sub.ai_eval);
                    let scoreBadge = `<span class="badge badge-warning">Awaiting Evaluation</span>`;

                    if (evalObj) {
                        const passBadge = evalObj.is_pass ? 'badge-success' : 'badge-danger';
                        scoreBadge = `<span class="badge ${passBadge}">${evalObj.total} / 100 (${evalObj.is_pass ? 'PASS' : 'FAIL'})</span>`;
                    }

                    const card = document.createElement('div');
                    card.className = 'project-card glass-card';
                    card.innerHTML = `
                        <div class="project-card-header">
                            <div>
                                <h4>${escapeHtml(sub.title)}</h4>
                                <small class="text-muted">${escapeHtml(sub.subject_name)} | Code: ${escapeHtml(sub.special_code)}</small>
                            </div>
                            ${scoreBadge}
                        </div>
                        <div class="project-card-body">
                            <p>${escapeHtml(sub.description)}</p>
                            <small class="text-dim"><i class="fa-solid fa-file-pdf"></i> ${escapeHtml(sub.pdf_filename)}</small>
                        </div>
                        <div class="project-card-footer">
                            <span class="text-muted" style="font-size: 11px;">Submitted: ${escapeHtml(sub.submitted_at)}</span>
                            <button class="btn btn-outline btn-sm view-result-btn" data-id="${sub.id}">
                                <i class="fa-solid fa-eye"></i> View Feedback
                            </button>
                        </div>
                    `;
                    grid.appendChild(card);
                });
            }

            document.getElementById('stuSubCount').textContent = subCount;
            document.getElementById('stuGradedCount').textContent = gradedCount;

            // Attach view result handlers
            document.querySelectorAll('.view-result-btn').forEach(b => {
                b.addEventListener('click', (e) => openStudentResultModal(e.target.closest('button').dataset.id));
            });
        } catch (e) {
            console.error('Error loading student submissions:', e);
        }
    }

    // Student: Enroll Modal Open
    document.getElementById('openEnrollModalBtn').addEventListener('click', async () => {
        try {
            const res = await fetch('/api/courses/list');
            const data = await res.json();
            const select = document.getElementById('enrollCourseSelect');
            select.innerHTML = '';
            if (data.success && data.courses) {
                data.courses.forEach(c => {
                    const opt = document.createElement('option');
                    opt.value = c.code;
                    opt.textContent = `${c.subject_name} (${c.faculty_name}) - Code: ${c.code}`;
                    select.appendChild(opt);
                });
            }
            enrollModal.classList.add('active');
        } catch (err) {
            alert('Could not fetch available courses.');
        }
    });

    document.getElementById('enrollForm').addEventListener('submit', async (e) => {
        e.preventDefault();
        const code = document.getElementById('enrollCodeInput').value;
        try {
            const res = await fetch('/api/courses/enroll', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({
                    code: code,
                    student_username: currentUser.username,
                    student_name: currentUser.full_name
                })
            });
            const data = await res.json();
            alert(data.message);
            if (data.success) {
                enrollModal.classList.remove('active');
                loadStudentDashboard();
            }
        } catch (err) {
            alert('Failed to send enrollment request.');
        }
    });

    // Student: Submit Project Modal Open
    document.getElementById('openSubmitModalBtn').addEventListener('click', async () => {
        try {
            const res = await fetch(`/api/courses/requests?student=${currentUser.username}`);
            const data = await res.json();
            const select = document.getElementById('submitCourseSelect');
            select.innerHTML = '';

            if (data.success && data.enrollments) {
                const accepted = data.enrollments.filter(e => e.status === 'accepted');
                if (accepted.length === 0) {
                    alert('You must be enrolled and accepted into at least one course before submitting a project.');
                    return;
                }
                accepted.forEach(c => {
                    const opt = document.createElement('option');
                    opt.value = c.course_id;
                    opt.textContent = `${c.subject_name} (Code: ${c.course_code})`;
                    select.appendChild(opt);
                });
            }
            submitModal.classList.add('active');
        } catch (err) {
            alert('Could not load course list.');
        }
    });

    // PDF File reader listener
    document.getElementById('subPdfFile').addEventListener('change', function(e) {
        const file = e.target.files[0];
        if (file) {
            const reader = new FileReader();
            reader.onload = function(evt) {
                document.getElementById('subPdfText').value = 
                    `TITLE: ${document.getElementById('subTitle').value || file.name}\n` +
                    `FILE: ${file.name}\n\n` +
                    evt.target.result;
            };
            reader.readAsText(file);
        }
    });

    document.getElementById('submitProjectForm').addEventListener('submit', async (e) => {
        e.preventDefault();
        const course_id = document.getElementById('submitCourseSelect').value;
        const title = document.getElementById('subTitle').value;
        const description = document.getElementById('subDescription').value;
        const special_code = document.getElementById('subCode').value;
        const fileInput = document.getElementById('subPdfFile');
        const pdf_filename = fileInput.files.length > 0 ? fileInput.files[0].name : `${title.replace(/\s+/g, '_')}_Report.pdf`;
        const pdf_content = document.getElementById('subPdfText').value;

        try {
            const res = await fetch('/api/submissions/create', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({
                    course_id: parseInt(course_id),
                    student_username: currentUser.username,
                    student_name: currentUser.full_name,
                    title,
                    description,
                    pdf_filename,
                    pdf_content,
                    special_code
                })
            });
            const data = await res.json();
            alert(data.message);
            if (data.success) {
                submitModal.classList.remove('active');
                loadStudentDashboard();
            }
        } catch (err) {
            alert('Error submitting project.');
        }
    });

    async function openStudentResultModal(subId) {
        try {
            const res = await fetch(`/api/submissions/detail?id=${subId}`);
            const data = await res.json();
            if (!data.success || !data.submission) return;

            const sub = data.submission;
            const evalObj = sub.has_final_eval ? sub.final_eval : (sub.has_faculty_eval ? sub.faculty_eval : sub.ai_eval);
            const container = document.getElementById('studentResultBody');

            if (!evalObj) {
                container.innerHTML = `
                    <div class="glass-panel" style="padding: 24px; text-align: center;">
                        <i class="fa-solid fa-clock-rotate-left" style="font-size: 36px; color: var(--warning); margin-bottom: 12px;"></i>
                        <h4>Evaluation Pending</h4>
                        <p class="text-muted">Your project submission '${escapeHtml(sub.title)}' has been delivered to your faculty and is queued for AI and online rubric evaluation.</p>
                    </div>
                `;
            } else {
                container.innerHTML = `
                    <div style="margin-bottom: 20px;">
                        <h4>${escapeHtml(sub.title)}</h4>
                        <p class="text-muted">${escapeHtml(sub.subject_name)} | Faculty: ${escapeHtml(sub.faculty_name)}</p>
                    </div>
                    
                    <div class="stats-grid" style="margin-bottom: 20px;">
                        <div class="stat-card glass-card">
                            <div class="stat-info">
                                <span class="stat-value" style="color: ${evalObj.is_pass ? 'var(--success)' : 'var(--danger)'};">${evalObj.total} / 100</span>
                                <span class="stat-label">Final Evaluation Score</span>
                            </div>
                        </div>
                        <div class="stat-card glass-card">
                            <div class="stat-info">
                                <span class="stat-value"><span class="badge ${evalObj.is_pass ? 'badge-success' : 'badge-danger'}" style="font-size: 16px;">${evalObj.is_pass ? 'PASS' : 'FAIL'}</span></span>
                                <span class="stat-label">Status Threshold (Pass >= 50)</span>
                            </div>
                        </div>
                    </div>

                    <div class="glass-panel" style="padding: 16px; margin-bottom: 20px;">
                        <h5>Rubric Score Breakdown</h5>
                        <table class="custom-table" style="margin-top: 12px;">
                            <thead>
                                <tr>
                                    <th>Rubric Criteria</th>
                                    <th>Max Marks</th>
                                    <th>Scored</th>
                                    <th>Detailed Evaluator Feedback</th>
                                </tr>
                            </thead>
                            <tbody>
                                <tr>
                                    <td>1. Existing Project & Novelty Check</td>
                                    <td>30</td>
                                    <td><strong>${evalObj.r1}</strong></td>
                                    <td><small>${escapeHtml(evalObj.r1_fb)}</small></td>
                                </tr>
                                <tr>
                                    <td>2. Code Quality & Techniques</td>
                                    <td>30</td>
                                    <td><strong>${evalObj.r2}</strong></td>
                                    <td><small>${escapeHtml(evalObj.r2_fb)}</small></td>
                                </tr>
                                <tr>
                                    <td>3. System Modules & Outputs</td>
                                    <td>20</td>
                                    <td><strong>${evalObj.r3}</strong></td>
                                    <td><small>${escapeHtml(evalObj.r3_fb)}</small></td>
                                </tr>
                                <tr>
                                    <td>4. Documentation & Scope</td>
                                    <td>20</td>
                                    <td><strong>${evalObj.r4}</strong></td>
                                    <td><small>${escapeHtml(evalObj.r4_fb)}</small></td>
                                </tr>
                            </tbody>
                        </table>
                    </div>

                    <div class="glass-card" style="padding: 16px;">
                        <h6 style="color: var(--primary); margin-bottom: 6px;">Evaluator Notes</h6>
                        <p style="font-size: 13px; color: var(--text-muted);">${escapeHtml(evalObj.notes)}</p>
                    </div>
                `;
            }
            studentResultModal.classList.add('active');
        } catch (err) {
            console.error('Error fetching submission result:', err);
        }
    }

    // ----------------------------------------------------
    // FACULTY DASHBOARD LOGIC
    // ----------------------------------------------------
    async function loadFacultyDashboard() {
        // 1. Fetch Analytics
        try {
            const res = await fetch(`/api/analytics?faculty=${currentUser.username}`);
            const data = await res.json();
            if (data.success && data.analytics) {
                const a = data.analytics;
                document.getElementById('kpiEnrolled').textContent = a.total_enrolled;
                document.getElementById('kpiSubmitted').textContent = a.total_submitted;
                document.getElementById('kpiPending').textContent = a.pending_grading;
                document.getElementById('kpiRatio').textContent = `${a.submission_ratio.toFixed(1)}%`;
                document.getElementById('kpiAvgScore').textContent = `${a.class_average.toFixed(1)} / 100`;

                // Update Grade Distribution Bars
                const totalGraded = a.graded_count || 1;
                document.getElementById('countA').textContent = a.grade_a;
                document.getElementById('countB').textContent = a.grade_b;
                document.getElementById('countC').textContent = a.grade_c;
                document.getElementById('countFail').textContent = a.grade_fail;

                document.getElementById('barA').style.width = `${(a.grade_a / totalGraded) * 100}%`;
                document.getElementById('barB').style.width = `${(a.grade_b / totalGraded) * 100}%`;
                document.getElementById('barC').style.width = `${(a.grade_c / totalGraded) * 100}%`;
                document.getElementById('barFail').style.width = `${(a.grade_fail / totalGraded) * 100}%`;
            }
        } catch (e) {
            console.error('Error loading faculty analytics:', e);
        }

        // 2. Fetch Enrollment Requests
        try {
            const res = await fetch(`/api/courses/requests?faculty=${currentUser.username}`);
            const data = await res.json();
            const table = document.getElementById('facultyEnrollmentRequestsTable');
            table.innerHTML = '';

            if (data.success && data.enrollments && data.enrollments.length > 0) {
                data.enrollments.forEach(en => {
                    const row = document.createElement('tr');
                    row.innerHTML = `
                        <td><strong>${escapeHtml(en.student_name)}</strong></td>
                        <td><code>${escapeHtml(en.student_username)}</code></td>
                        <td>${escapeHtml(en.subject_name)}</td>
                        <td><code>${escapeHtml(en.course_code)}</code></td>
                        <td>${escapeHtml(en.requested_at)}</td>
                        <td>
                            <button class="btn btn-success-sm approve-btn" data-id="${en.id}" data-status="accepted">
                                <i class="fa-solid fa-check"></i> Accept
                            </button>
                            <button class="btn btn-danger-sm approve-btn" data-id="${en.id}" data-status="rejected">
                                <i class="fa-solid fa-xmark"></i> Reject
                            </button>
                        </td>
                    `;
                    table.appendChild(row);
                });

                document.querySelectorAll('.approve-btn').forEach(btn => {
                    btn.addEventListener('click', async (e) => {
                        const target = e.target.closest('button');
                        const id = target.dataset.id;
                        const status = target.dataset.status;
                        await updateEnrollment(id, status);
                    });
                });
            } else {
                table.innerHTML = `<tr><td colspan="6" class="text-muted" style="text-align: center;">No pending enrollment requests.</td></tr>`;
            }
        } catch (e) {
            console.error('Error loading enrollment requests:', e);
        }

        // 3. Fetch Submissions Queue
        try {
            const res = await fetch(`/api/submissions/list?faculty=${currentUser.username}`);
            const data = await res.json();
            const table = document.getElementById('facultySubmissionsTable');
            table.innerHTML = '';

            if (data.success && data.submissions && data.submissions.length > 0) {
                data.submissions.forEach(sub => {
                    const aiStatus = sub.has_ai_eval ? `<span class="badge badge-ai"><i class="fa-solid fa-robot"></i> Evaluated (${sub.ai_eval.total})</span>` : `<span class="badge badge-warning">Pending AI</span>`;
                    
                    let finalBadge = `<span class="badge badge-warning">Ungraded</span>`;
                    const evalObj = sub.has_final_eval ? sub.final_eval : (sub.has_faculty_eval ? sub.faculty_eval : null);
                    if (evalObj) {
                        finalBadge = `<span class="badge ${evalObj.is_pass ? 'badge-success' : 'badge-danger'}">${evalObj.total} / 100</span>`;
                    }

                    const row = document.createElement('tr');
                    row.innerHTML = `
                        <td>#${sub.id}</td>
                        <td><strong>${escapeHtml(sub.student_name)}</strong><br><small class="text-muted">${escapeHtml(sub.student_username)}</small></td>
                        <td>${escapeHtml(sub.title)}<br><small class="text-dim">${escapeHtml(sub.subject_name)}</small></td>
                        <td><small><i class="fa-solid fa-file-pdf"></i> ${escapeHtml(sub.pdf_filename)}</small></td>
                        <td><code>${escapeHtml(sub.special_code)}</code></td>
                        <td><small>${escapeHtml(sub.submitted_at)}</small></td>
                        <td>${aiStatus}</td>
                        <td>${finalBadge}</td>
                        <td>
                            <button class="btn btn-primary btn-sm open-eval-hub-btn" data-id="${sub.id}">
                                <i class="fa-solid fa-sliders"></i> Evaluate & Chat
                            </button>
                        </td>
                    `;
                    table.appendChild(row);
                });

                document.querySelectorAll('.open-eval-hub-btn').forEach(btn => {
                    btn.addEventListener('click', (e) => openEvalHubModal(e.target.closest('button').dataset.id));
                });
            } else {
                table.innerHTML = `<tr><td colspan="9" class="text-muted" style="text-align: center;">No student submissions yet.</td></tr>`;
            }
        } catch (e) {
            console.error('Error loading faculty submissions roster:', e);
        }
    }

    async function updateEnrollment(enrollment_id, status) {
        try {
            const res = await fetch('/api/courses/approve', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ enrollment_id: parseInt(enrollment_id), status })
            });
            const data = await res.json();
            alert(data.message);
            loadFacultyDashboard();
        } catch (err) {
            alert('Failed to update enrollment request.');
        }
    }

    // Faculty: Create Course Modal
    document.getElementById('openCreateCourseModalBtn').addEventListener('click', () => {
        document.getElementById('newCourseCode').value = Math.floor(10000 + Math.random() * 90000).toString();
        createCourseModal.classList.add('active');
    });

    document.getElementById('generateCodeBtn').addEventListener('click', () => {
        document.getElementById('newCourseCode').value = Math.floor(10000 + Math.random() * 90000).toString();
    });

    document.getElementById('createCourseForm').addEventListener('submit', async (e) => {
        e.preventDefault();
        const subject_name = document.getElementById('newSubjectName').value;
        const code = document.getElementById('newCourseCode').value;

        try {
            const res = await fetch('/api/courses/create', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({
                    subject_name,
                    faculty_username: currentUser.username,
                    faculty_name: currentUser.full_name,
                    code
                })
            });
            const data = await res.json();
            alert(data.message);
            if (data.success) {
                createCourseModal.classList.remove('active');
                loadFacultyDashboard();
            }
        } catch (err) {
            alert('Error creating course.');
        }
    });

    // Excel Export Trigger
    document.getElementById('exportExcelBtn').addEventListener('click', () => {
        window.location.href = `/api/export/excel?faculty=${currentUser.username}`;
    });

    // ----------------------------------------------------
    // EVALUATION HUB & INTERACTIVE AI CHAT
    // ----------------------------------------------------
    const r1Slider = document.getElementById('r1Slider');
    const r2Slider = document.getElementById('r2Slider');
    const r3Slider = document.getElementById('r3Slider');
    const r4Slider = document.getElementById('r4Slider');

    const r1Value = document.getElementById('r1Value');
    const r2Value = document.getElementById('r2Value');
    const r3Value = document.getElementById('r3Value');
    const r4Value = document.getElementById('r4Value');

    const totalScoreDisplay = document.getElementById('totalScoreDisplay');
    const totalPassStatus = document.getElementById('totalPassStatus');

    function updateCalculatedTotal() {
        const r1 = parseInt(r1Slider.value);
        const r2 = parseInt(r2Slider.value);
        const r3 = parseInt(r3Slider.value);
        const r4 = parseInt(r4Slider.value);

        r1Value.textContent = `${r1} / 30`;
        r2Value.textContent = `${r2} / 30`;
        r3Value.textContent = `${r3} / 20`;
        r4Value.textContent = `${r4} / 20`;

        const total = r1 + r2 + r3 + r4;
        totalScoreDisplay.textContent = `${total} / 100`;

        if (total >= 50) {
            totalPassStatus.textContent = 'PASS (>= 50)';
            totalPassStatus.style.color = 'var(--success)';
        } else {
            totalPassStatus.textContent = 'FAIL (< 50)';
            totalPassStatus.style.color = 'var(--danger)';
        }
    }

    [r1Slider, r2Slider, r3Slider, r4Slider].forEach(s => s.addEventListener('input', updateCalculatedTotal));

    async function openEvalHubModal(subId) {
        activeSubmissionId = parseInt(subId);
        try {
            const res = await fetch(`/api/submissions/detail?id=${subId}`);
            const data = await res.json();
            if (!data.success || !data.submission) return;

            const sub = data.submission;
            document.getElementById('evalModalTitle').innerHTML = `<i class="fa-solid fa-microchip"></i> Evaluating: ${escapeHtml(sub.title)}`;
            document.getElementById('evalModalSubtitle').textContent = `Student: ${sub.student_name} (${sub.student_username}) | Special Verification Code: ${sub.special_code}`;

            // Render PDF Content
            document.getElementById('pdfFilenameBadge').textContent = sub.pdf_filename;
            document.getElementById('pdfContentDisplay').textContent = sub.pdf_content;

            // Load Existing Evaluations
            const evalObj = sub.has_faculty_eval ? sub.faculty_eval : (sub.has_ai_eval ? sub.ai_eval : null);
            if (evalObj) {
                r1Slider.value = evalObj.r1;
                r2Slider.value = evalObj.r2;
                r3Slider.value = evalObj.r3;
                r4Slider.value = evalObj.r4;

                document.getElementById('r1Feedback').value = evalObj.r1_fb || '';
                document.getElementById('r2Feedback').value = evalObj.r2_fb || '';
                document.getElementById('r3Feedback').value = evalObj.r3_fb || '';
                document.getElementById('r4Feedback').value = evalObj.r4_fb || '';
            } else {
                r1Slider.value = 25;
                r2Slider.value = 25;
                r3Slider.value = 15;
                r4Slider.value = 15;
            }
            updateCalculatedTotal();

            // Load Chat History
            await loadChatHistory(activeSubmissionId);

            evalHubModal.classList.add('active');
        } catch (err) {
            console.error('Error opening evaluation hub:', err);
        }
    }

    // AI Evaluation Trigger
    document.getElementById('triggerAiEvalBtn').addEventListener('click', async () => {
        if (!activeSubmissionId) return;
        const loader = document.getElementById('aiLoadingBar');
        loader.style.display = 'flex';

        try {
            const res = await fetch('/api/evaluate/ai', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ submission_id: activeSubmissionId })
            });
            const data = await res.json();
            loader.style.display = 'none';

            if (data.success && data.evaluation) {
                const ev = data.evaluation;
                r1Slider.value = ev.r1;
                r2Slider.value = ev.r2;
                r3Slider.value = ev.r3;
                r4Slider.value = ev.r4;

                document.getElementById('r1Feedback').value = ev.r1_fb;
                document.getElementById('r2Feedback').value = ev.r2_fb;
                document.getElementById('r3Feedback').value = ev.r3_fb;
                document.getElementById('r4Feedback').value = ev.r4_fb;

                updateCalculatedTotal();
                await loadChatHistory(activeSubmissionId);
                alert('AI Evaluation completed according to mandatory rubrics!');
            } else {
                alert('AI Evaluation error.');
            }
        } catch (err) {
            loader.style.display = 'none';
            alert('Failed to trigger AI evaluation.');
        }
    });

    // Chat History & Send
    async function loadChatHistory(subId) {
        try {
            const res = await fetch(`/api/chat/history?submission_id=${subId}`);
            const data = await res.json();
            const chatBox = document.getElementById('chatBox');
            chatBox.innerHTML = '';

            if (data.success && data.messages) {
                data.messages.forEach(m => {
                    const bubble = document.createElement('div');
                    bubble.className = `chat-bubble ${m.sender === 'FACULTY' ? 'chat-faculty' : 'chat-ai'}`;
                    bubble.innerHTML = `
                        <span class="chat-sender-tag">${m.sender}</span>
                        ${escapeHtml(m.message).replace(/\n/g, '<br>')}
                    `;
                    chatBox.appendChild(bubble);
                });
                chatBox.scrollTop = chatBox.scrollHeight;
            }
        } catch (err) {
            console.error('Error loading chat history:', err);
        }
    }

    document.getElementById('chatForm').addEventListener('submit', async (e) => {
        e.preventDefault();
        const input = document.getElementById('chatInput');
        const message = input.value.trim();
        if (!message || !activeSubmissionId) return;

        input.value = '';

        try {
            const res = await fetch('/api/chat/send', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({
                    submission_id: activeSubmissionId,
                    message: message
                })
            });
            const data = await res.json();
            if (data.success) {
                await loadChatHistory(activeSubmissionId);
                if (data.reply.suggested_r1) {
                    r1Slider.value = data.reply.suggested_r1;
                    r2Slider.value = data.reply.suggested_r2;
                    r3Slider.value = data.reply.suggested_r3;
                    r4Slider.value = data.reply.suggested_r4;
                    updateCalculatedTotal();
                }
            }
        } catch (err) {
            console.error('Error sending chat message:', err);
        }
    });

    // Faculty Save Evaluation Form
    document.getElementById('facultyEvalForm').addEventListener('submit', async (e) => {
        e.preventDefault();
        if (!activeSubmissionId) return;

        const body = {
            submission_id: activeSubmissionId,
            is_final: "false",
            rubric1_score: parseInt(r1Slider.value),
            rubric1_feedback: document.getElementById('r1Feedback').value,
            rubric2_score: parseInt(r2Slider.value),
            rubric2_feedback: document.getElementById('r2Feedback').value,
            rubric3_score: parseInt(r3Slider.value),
            rubric3_feedback: document.getElementById('r3Feedback').value,
            rubric4_score: parseInt(r4Slider.value),
            rubric4_feedback: document.getElementById('r4Feedback').value,
            evaluator_notes: "Faculty online evaluation saved."
        };

        try {
            const res = await fetch('/api/evaluate/faculty', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify(body)
            });
            const data = await res.json();
            alert(data.message);
            if (data.success) {
                loadFacultyDashboard();
            }
        } catch (err) {
            alert('Failed to save evaluation.');
        }
    });

    // Finalize Grade Button
    document.getElementById('finalizeGradeBtn').addEventListener('click', async () => {
        if (!activeSubmissionId) return;
        if (!confirm('Are you sure you want to finalize and lock the agreed final marks for this student?')) return;

        const body = {
            submission_id: activeSubmissionId,
            is_final: "true",
            rubric1_score: parseInt(r1Slider.value),
            rubric1_feedback: document.getElementById('r1Feedback').value,
            rubric2_score: parseInt(r2Slider.value),
            rubric2_feedback: document.getElementById('r2Feedback').value,
            rubric3_score: parseInt(r3Slider.value),
            rubric3_feedback: document.getElementById('r3Feedback').value,
            rubric4_score: parseInt(r4Slider.value),
            rubric4_feedback: document.getElementById('r4Feedback').value,
            evaluator_notes: "Final grade agreed and published after Faculty & AI dialogue."
        };

        try {
            const res = await fetch('/api/evaluate/faculty', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify(body)
            });
            const data = await res.json();
            alert('Grade finalized and published to student portal!');
            evalHubModal.classList.remove('active');
            loadFacultyDashboard();
        } catch (err) {
            alert('Failed to finalize grade.');
        }
    });

    // Utility: HTML Escape
    function escapeHtml(str) {
        if (!str) return '';
        return String(str)
            .replace(/&/g, '&amp;')
            .replace(/</g, '&lt;')
            .replace(/>/g, '&gt;')
            .replace(/"/g, '&quot;')
            .replace(/'/g, '&#039;');
    }
});
