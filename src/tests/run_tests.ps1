# Note: Chatgpt wrote most of this because I can't be arsed to learn powershell.

# Display usage information
function Usage {
    Write-Host "Usage: $(basename $MyInvocation.MyCommand) [-h|-d] /path/to/ploy /path/to/test/cases/dir"
    Write-Host "Options:"
    Write-Host "    -h      Display help message and quit."
    Write-Host "    -d      Skip output checks and show disassembly of all test cases."
}

# Initialize variables
$disassemble = $false
$errors_encountered = $false

# Check the first argument
if ($args[0] -eq '-h') {
    Usage
    exit
}
elseif ($args[0] -eq '-d') {
    $disassemble = $true
    $args = $args[1..($args.Length - 1)]  # Remove the -d argument from the list
}

# Validate input arguments
if ($args.Length -ne 2) {
    Write-Host "error: missing arguments"
    Usage
    exit 1
}

$ploy_exe = $args[0]
$test_cases_dir = $args[1]

# Process each test case
Get-ChildItem -Path $test_cases_dir -Filter *.scm | ForEach-Object {
    $test_case = $_.FullName

    if ($disassemble) {
        & $ploy_exe -d $test_case
        if ($LASTEXITCODE -ne 0) {
            $errors_encountered = $true
        }
        return
    }

    # Run the test case
    $test_case_output = & $ploy_exe $test_case | Out-String

    if ($LASTEXITCODE -ne 0) {
        $errors_encountered = $true
        Write-Host "error: test case $test_case returned non-zero status:`n$test_case_output"
        return
    }

    # Expected output (from the comments in the file)
    $expected_output = (Select-String -Path $test_case -Pattern ';; (.+)' | ForEach-Object { $_.Matches.Groups[1].Value }) | Out-String

    if ($test_case_output -ne $expected_output) {
        $errors_encountered = $true
        Write-Host "error: test case $test_case failed:"
        Write-Host -NoNewline "expected output:`n$expected_output"
        Write-Host -NoNewline "actual output:`n$test_case_output"
    }
}

# Final check for errors
if ($errors_encountered) {
    Write-Host "one or more errors has occurred"
    exit 1
}
