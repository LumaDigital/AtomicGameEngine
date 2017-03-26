using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;

namespace AtomicEngine
{

    [ComVisible(true)]
    public partial class RefCounted : IDisposable
    {

        public bool Disposed = false;

        public RefCounted()
        {

        }

        protected RefCounted(IntPtr native)
        {
            nativeInstance = native;
        }

        public void Dispose()
        {
            Dispose(true);
            GC.SuppressFinalize(this);            
        }

        protected virtual void Dispose(bool disposing)
        {
            // guard against double explicit/using statement dispose
            if (Disposed)
                return;

            Disposed = true;

            if (nativeInstance == IntPtr.Zero)
            {
                throw new InvalidOperationException("RefCounted.Dispose - Invalid native instance == IntPtr.Zero, double call to Dispose?");
            }

            if (disposing)
            {
                // must be on main engine thread!

                if (NativeCore.csi_Atomic_RefCounted_Refs(nativeInstance) < 1)
                {
                    throw new InvalidOperationException("RefCounted.Dispose - Native instance has less than 1 reference count");

                }

                NativeCore.RemoveNative(nativeInstance);

            }

            nativeInstance = IntPtr.Zero;

        }

        ~RefCounted()
        {
            // This can run on any thread, so queue release
            lock (refCountedFinalizerQueue)
                refCountedFinalizerQueue.Add(nativeInstance);
        }

        static internal List<IntPtr> refCountedFinalizerQueue = new List<IntPtr>();

        static internal void ReleaseFinalized()
        {
            lock (refCountedFinalizerQueue)
            {
                foreach (var native in refCountedFinalizerQueue)
                {
                    NativeCore.RemoveNative(native);
                }

                refCountedFinalizerQueue.Clear();
            }

        }

        // This method may be called multiple times, called on instance after it is either registered as a new native created in C# (InstantiationType == InstantiationType.INSTANTIATION_NET)
        // or a native which has been wrapped ((InstantiationType != InstantiationType.INSTANTIATION_NET)
        // Note that RefCounted that get GC'd from script, can still live in native code, and get rewrapped 
        internal virtual void PostNativeUpdate()
        {

        }

        public static implicit operator IntPtr(RefCounted refCounted)
        {
            if (refCounted == null)
                return IntPtr.Zero;
                
            return refCounted.nativeInstance;
        }

        public IntPtr NativeInstance { get { return nativeInstance; } }

        public IntPtr nativeInstance = IntPtr.Zero;

        [DllImport(Constants.LIBNAME, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
        public static extern IntPtr csi_Atomic_RefCounted_GetClassID(IntPtr self);

    }


}
