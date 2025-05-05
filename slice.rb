require 'optparse'

options = {
  start: nil,
  size: nil,
  file: nil,
  full_lines_only: false
}

OptionParser.new do |opts|
  opts.banner = "Usage: slice.rb --start OFFSET --size BYTES --file FILENAME [--full-lines-only]"

  opts.on("--start OFFSET", Integer, "Byte offset to start reading") { |v| options[:start] = v }
  opts.on("--size BYTES", Integer, "Number of bytes to read") { |v| options[:size] = v }
  opts.on("--file FILENAME", String, "File to extract from") { |v| options[:file] = v }
  opts.on("--full-lines-only", "Remove truncated lines at start/end") { options[:full_lines_only] = true }
  opts.on("--help", "Prints this help") do
    puts opts
    exit
  end
end.parse!

if options[:file].nil? || !File.file?(options[:file])
  STDERR.puts "Error: File not found or invalid."
  exit 1
end

if options[:start].nil? || options[:size].nil? || options[:start] < 0 || options[:size] <= 0
  STDERR.puts "Error: --start must be >= 0 and --size must be > 0."
  exit 1
end

def read_slice(filename, start, size)
  File.open(filename, 'rb') do |f|
    f.seek(start)
    f.read(size) || ""
  end
rescue => e
  STDERR.puts "Failed to read file: #{e.message}"
  exit 1
end

def trim_truncated_lines(data, start)
  lines = data.lines
  lines.shift if start > 0 && lines.any?
  lines.pop unless lines.empty? || lines.last.end_with?("\n")
  lines.join
end

begin
  data = read_slice(options[:file], options[:start], options[:size])
  data = trim_truncated_lines(data, options[:start]) if options[:full_lines_only]
  STDOUT.write(data)
rescue => e
  STDERR.puts "Unexpected error: #{e.message}"
  exit 1
end
