#!/usr/bin/env ruby
require 'open3'
require 'fileutils'

SLICE_CMD = ["ruby", "../../slice.rb"]
TEST_FILE = "test.txt"

def run_test(name)
  puts "=== RUN   #{name}"
  begin
    yield
    puts "--- PASS: #{name}"
  rescue => e
    puts "--- FAIL: #{name}"
    puts "    #{e.message}"
    exit 1
  ensure
    FileUtils.rm_f(TEST_FILE)
  end
end

def write_file(content)
  File.open(TEST_FILE, "wb") { |f| f.write(content) }
end

def run_slice(*args)
  Open3.capture3(*SLICE_CMD + args)
end

run_test("TestBasicSlice") do
  write_file("Line 1\nLine 2\nLine 3\n")
  stdout, stderr, status = run_slice("--start", "7", "--size", "7", "--file", TEST_FILE)
  raise "Expected 'Line 2\\n', got: #{stdout.inspect}" unless stdout == "Line 2\n"
  raise "Expected success, got: #{stderr}" unless status.success?
end

run_test("TestStartAtZero") do
  write_file("abcdefghij")
  stdout, _, _ = run_slice("--start", "0", "--size", "5", "--file", TEST_FILE)
  raise "Expected 'abcde', got: #{stdout.inspect}" unless stdout == "abcde"
end

run_test("TestSliceAtEOF") do
  write_file("1234567890")
  stdout, _, _ = run_slice("--start", "8", "--size", "4", "--file", TEST_FILE)
  raise "Expected '90', got: #{stdout.inspect}" unless stdout.strip == "90"
end

run_test("TestOversizedSlice") do
  write_file("abcdefghij")
  stdout, _, _ = run_slice("--start", "0", "--size", "100", "--file", TEST_FILE)
  raise "Expected full content, got: #{stdout.inspect}" unless stdout == "abcdefghij"
end

run_test("TestMissingFile") do
  _, stderr, status = run_slice("--start", "0", "--size", "10", "--file", "nonexistent.txt")
  raise "Expected failure for missing file" if status.success?
  raise "Expected error message, got: #{stderr.inspect}" unless stderr.downcase.include?("error")
end

run_test("TestMissingArguments") do
  _, stderr, status = run_slice("--start", "0", "--size", "10")
  raise "Expected failure due to missing args" if status.success?
  raise "Expected error message, got: #{stderr.inspect}" unless stderr.downcase.include?("error")
end

puts "PASS"
