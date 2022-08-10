
module RawDhWrapper
	using CxxWrap
	@wrapmodule(joinpath("build/lib/", "libdhwrapper.so"))

	function __init__()
		@initcxx
	end
end

module DhWrapper
	import ..RawDhWrapper
	import CxxWrap
	import Arrow

	TableHandle = RawDhWrapper.TableHandleAllocated
	TableHandleManager = RawDhWrapper.TableHandleManagerAllocated

	function clientconnect(hostname)
		return RawDhWrapper.clientconnect(hostname)
	end

	function manager(client)
		return RawDhWrapper.getManager(client)
	end

	function emptytable(mgr::TableHandleManager, rows::Int64)::TableHandle
		return RawDhWrapper.emptyTable(mgr, rows)
	end

	function update(tblhandle::TableHandle, formulas)::TableHandle
		f = CxxWrap.StdLib.StdVector([CxxWrap.StdLib.StdString(f) for f in formulas])
		return RawDhWrapper.update(tblhandle, f)
	end

	function show(tblhandle::TableHandle, headers::Bool)
		return RawDhWrapper.show(tblhandle, headers)
	end

	function importtable(mgr::TableHandleManager, arrowtbl)::TableHandle
		bytebuf = IOBuffer()

		Arrow.write(bytebuf, arrowtbl)
		seekstart(bytebuf)
		bytes = read(bytebuf, String)

		return RawDhWrapper.deserializeandupload(mgr, Vector{UInt8}(bytes))
	end

	function snapshot(tblhandle::TableHandle)
		ser = RawDhWrapper.TableSerializer(tblhandle)
		arr = copy(RawDhWrapper.dataarr(ser))
		return Arrow.Stream(arr)
	end
end

using Tables

using CxxWrap

using Arrow

function main()
	client = DhWrapper.clientconnect("localhost:10000")
	mgr = DhWrapper.manager(client)

	tbl1 = DhWrapper.emptytable(mgr, 10)

	try
		tbl2 = DhWrapper.update(tbl1, ["JL = ii + 100"])

		println(DhWrapper.show(tbl2, true))

		arrowtbl = DhWrapper.snapshot(tbl2)

		for column in Tables.columns(arrowtbl)
			println(column)
		end

		newhandle = DhWrapper.importtable(mgr, arrowtbl)
	catch e
		println("Got an error ", e)
	end
end

main()